#include "track/v4l2dma_track_source.h"

#include <future>

// WebRTC
#include <api/video/i420_buffer.h>
#include <api/video/video_frame_buffer.h>
#include <rtc_base/timestamp_aligner.h>
#include <third_party/libyuv/include/libyuv.h>

#include "v4l2_codecs/raw_buffer.h"

rtc::scoped_refptr<V4l2DmaTrackSource>
V4l2DmaTrackSource::Create(std::shared_ptr<V4L2Capture> capture) {
    auto obj = rtc::make_ref_counted<V4l2DmaTrackSource>(std::move(capture));
    obj->Init();
    obj->StartTrack();
    return obj;
}

V4l2DmaTrackSource::V4l2DmaTrackSource(std::shared_ptr<V4L2Capture> capture)
    : ScaleTrackSource(capture),
      config_width(capture->width()),
      config_height(capture->height()),
      format(capture->format()) {}

V4l2DmaTrackSource::~V4l2DmaTrackSource() {
    scaler.reset();
    decoder.reset();
}

void V4l2DmaTrackSource::Init() {
    scaler = std::make_unique<V4l2Scaler>();
    scaler->Configure(width, height, width, height, true, true);
    scaler->Start();

    decoder = std::make_unique<V4l2Decoder>();
    decoder->Configure(width, height, format, true);
    decoder->Start();
}

bool V4l2DmaTrackSource::HasFirstKeyFrame(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) {
    return true;
}

void V4l2DmaTrackSource::OnFrameCaptured(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) {
    if (!HasFirstKeyFrame(frame_buffer)) {
        return;
    }

    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
        timestamp_aligner.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    V4l2Buffer buffer((void *)frame_buffer->Data(), frame_buffer->size(), frame_buffer->flags(),
                      frame_buffer->timestamp());

    decoder->EmplaceBuffer(buffer, [this, timestamp_us,
                                    translated_timestamp_us](V4l2Buffer decoded_buffer) {
        if (!i420_raw_buffer) {
            i420_raw_buffer =
                RawBuffer::Create(width, height, decoded_buffer.length, decoded_buffer);
        }

        int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
        if (!AdaptFrame(width, height, timestamp_us, &adapted_width, &adapted_height, &crop_width,
                        &crop_height, &crop_x, &crop_y)) {
            return;
        }

        if (adapted_width != config_width || adapted_height != config_height) {
            config_width = adapted_width;
            config_height = adapted_height;
            scaler->ReleaseCodec();
            scaler->Configure(width, height, config_width, config_height, true, true);
            scaler->Start();
        }

        scaler->EmplaceBuffer(
            decoded_buffer, [this, translated_timestamp_us](V4l2Buffer scaled_buffer) {
                auto dst_buffer = RawBuffer::Create(config_width, config_height, 0, scaled_buffer);

                OnFrame(webrtc::VideoFrame::Builder()
                            .set_video_frame_buffer(dst_buffer)
                            .set_rotation(webrtc::kVideoRotation_0)
                            .set_timestamp_us(translated_timestamp_us)
                            .build());
            });
    });
}
