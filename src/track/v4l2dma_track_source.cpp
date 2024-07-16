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
    obj->Init();
    obj->StartTrack();
    return obj;
}

V4l2DmaTrackSource::V4l2DmaTrackSource(std::shared_ptr<V4L2Capture> capture)
    : SwScaleTrackSource(capture),
      is_dma_(capture_->is_dma()),
      format_(capture_->format()),
      has_first_keyframe_(false) {}

V4l2DmaTrackSource::~V4l2DmaTrackSource() {
    scaler_.reset();
    decoder_.reset();
}

void V4l2DmaTrackSource::Init() {
    scaler_ = std::make_unique<V4l2Scaler>();
    scaler_->Configure(width_, height_, width_, height_, true, is_dma_);
    scaler_->Start();

    decoder_ = std::make_unique<V4l2Decoder>();
    decoder_->Configure(width_, height_, format_, true);
    decoder_->Start();
}

void V4l2DmaTrackSource::OnFrameCaptured(rtc::scoped_refptr<V4l2FrameBuffer> &frame_buffer) {

    if (!has_first_keyframe_) {
        if (frame_buffer->flags() & V4L2_BUF_FLAG_KEYFRAME) {
            has_first_keyframe_ = true;
        } else {
            return;
        }
    }

    V4l2Buffer buffer((void*)frame_buffer->Data(), frame_buffer->size(),
                          frame_buffer->flags(), frame_buffer->timestamp());

    decoder_->EmplaceBuffer(buffer, [this](V4l2Buffer decoded_buffer) {

        if (!i420_raw_buffer_) {
            i420_raw_buffer_ = RawBuffer::Create(width_, height_, decoded_buffer.length, decoded_buffer);
        }

        static rtc::TimestampAligner timestamp_aligner_;
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
                                config_height_, true, is_dma_);
            scaler_->Start();
            if (format_ == V4L2_PIX_FMT_MJPEG) {
                decoder_->ReleaseCodec();
                decoder_->Configure(width_, height_, format_, true);
                decoder_->Start();
            }
        }

        scaler_->EmplaceBuffer(decoded_buffer, [this, translated_timestamp_us](V4l2Buffer scaled_buffer) {

            auto dst_buffer = RawBuffer::Create(config_width_, config_height_, 0, scaled_buffer);

            OnFrame(webrtc::VideoFrame::Builder()
                    .set_video_frame_buffer(dst_buffer)
                    .set_rotation(webrtc::kVideoRotation_0)
                    .set_timestamp_us(translated_timestamp_us)
                    .build());
        });
    });
}
