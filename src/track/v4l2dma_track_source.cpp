#include "track/v4l2dma_track_source.h"

#include <future>

// WebRTC
#include <api/video/i420_buffer.h>
#include <api/video/video_frame_buffer.h>
#include <rtc_base/timestamp_aligner.h>
#include <third_party/libyuv/include/libyuv.h>

#include "v4l2_codecs/raw_frame_buffer.h"

rtc::scoped_refptr<V4l2DmaTrackSource>
V4l2DmaTrackSource::Create(std::shared_ptr<V4L2Capture> capture) {
    auto obj = rtc::make_ref_counted<V4l2DmaTrackSource>(std::move(capture));
    obj->Init();
    obj->StartTrack();
    return obj;
}

V4l2DmaTrackSource::V4l2DmaTrackSource(std::shared_ptr<V4L2Capture> capture)
    : ScaleTrackSource(capture),
      is_dma_src_(capture->is_dma_capture()),
      config_width_(capture->width()),
      config_height_(capture->height()) {}

V4l2DmaTrackSource::~V4l2DmaTrackSource() {
    scaler.reset();
}

void V4l2DmaTrackSource::Init() {
    scaler = std::make_unique<V4l2Scaler>();
    scaler->Configure(width, height, width, height, is_dma_src_, true);
    scaler->Start();
}

void V4l2DmaTrackSource::StartTrack() {
    auto observer = capture->AsYuvBufferObservable();
    observer->Subscribe([this](rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer) {
        i420_raw_buffer = frame_buffer;
        RawFrameBuffer *raw_buffer = static_cast<RawFrameBuffer *>(frame_buffer.get());
        OnFrameCaptured(raw_buffer->GetBuffer());
    });
}

void V4l2DmaTrackSource::OnFrameCaptured(V4l2Buffer decoded_buffer) {
    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
    timestamp_aligner.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
    if (!AdaptFrame(width, height, timestamp_us, &adapted_width, &adapted_height, &crop_width,
                    &crop_height, &crop_x, &crop_y)) {
        return;
    }

    if (adapted_width != config_width_ || adapted_height != config_height_) {
        config_width_ = adapted_width;
        config_height_ = adapted_height;
        scaler->ReleaseCodec();
        scaler->Configure(width, height, config_width_, config_height_, is_dma_src_, true);
        scaler->Start();
    }

    scaler->EmplaceBuffer(
        decoded_buffer, [this, translated_timestamp_us](V4l2Buffer scaled_buffer) {
            auto dst_buffer = RawFrameBuffer::Create(config_width_, config_height_, scaled_buffer);

            OnFrame(webrtc::VideoFrame::Builder()
                        .set_video_frame_buffer(dst_buffer)
                        .set_rotation(webrtc::kVideoRotation_0)
                        .set_timestamp_us(translated_timestamp_us)
                        .build());
        });
}
