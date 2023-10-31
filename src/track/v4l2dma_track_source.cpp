#include "v4l2dma_track_source.h"
#include "v4l2_codecs/raw_buffer.h"

#include <future>

// WebRTC
#include <api/video/i420_buffer.h>
#include <api/video/video_frame_buffer.h>
#include <rtc_base/timestamp_aligner.h>
#include <third_party/libyuv/include/libyuv.h>

rtc::scoped_refptr<V4l2DmaTrackSource> V4l2DmaTrackSource::Create(
    std::shared_ptr<V4L2Capture> capture) {
    auto obj = rtc::make_ref_counted<V4l2DmaTrackSource>(std::move(capture));
    obj->StartTrack();
    return obj;
}

V4l2DmaTrackSource::V4l2DmaTrackSource(std::shared_ptr<V4L2Capture> capture)
    : V4L2TrackSource(capture) {}

void V4l2DmaTrackSource::Init() {
    auto s = std::async(std::launch::async, [&]() {
        scaler_ = std::make_unique<V4l2Scaler>();
        scaler_->Configure(width_, height_, width_, height_, true, capture_->is_dma());
    });

    auto d = std::async(std::launch::async, [&]() {
        decoder_ = std::make_unique<V4l2Decoder>();
        decoder_->Configure(width_, height_, capture_->format(), true);
    });
}

void V4l2DmaTrackSource::OnFrameCaptured(Buffer buffer) {
    rtc::TimestampAligner timestamp_aligner_;
    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
        timestamp_aligner_.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
    if (!AdaptFrame(width_, height_, timestamp_us, &adapted_width, &adapted_height,
                    &crop_width, &crop_height, &crop_x, &crop_y)) {
        return;
    }

    if (adapted_width != config_width_ || adapted_height != config_height_) {
        config_width_ = adapted_width;
        config_height_ = adapted_height;
        scaler_->ReleaseCodec();
        scaler_->Configure(width_, height_, config_width_,
                            config_height_, true, capture_->is_dma());
        if (capture_->format() == V4L2_PIX_FMT_MJPEG) {
            decoder_->ReleaseCodec();
            decoder_->Configure(width_, height_, capture_->format(), true);
        }
    }

    decoder_->EmplaceBuffer(buffer, [&](Buffer decoded_buffer) {
        scaler_->EmplaceBuffer(decoded_buffer, [&](Buffer scaled_buffer) {
            rtc::scoped_refptr<webrtc::VideoFrameBuffer> dst_buffer = nullptr;

            if (!capture_->is_dma()) {
                auto i420_buffer = webrtc::I420Buffer::Create(config_width_, config_height_);
                memcpy(i420_buffer->MutableDataY(), (uint8_t*)scaled_buffer.start, scaled_buffer.length);
                dst_buffer = i420_buffer;
            } else {
                dst_buffer = RawBuffer::Create(config_width_, config_height_,
                                                0, scaled_buffer);
            }

            OnFrame(webrtc::VideoFrame::Builder()
                    .set_video_frame_buffer(dst_buffer)
                    .set_rotation(webrtc::kVideoRotation_0)
                    .set_timestamp_us(translated_timestamp_us)
                    .build());
        });
    });
}
