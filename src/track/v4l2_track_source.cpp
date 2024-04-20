#include "v4l2_track_source.h"

#include <cmath>

// WebRTC
#include <api/video/i420_buffer.h>
#include <api/video/video_frame_buffer.h>
#include <rtc_base/timestamp_aligner.h>
#include <third_party/libyuv/include/libyuv.h>

static const int kBufferAlignment = 64;

rtc::scoped_refptr<V4L2TrackSource> V4L2TrackSource::Create(
    std::shared_ptr<V4L2Capture> capture) {
    auto obj = rtc::make_ref_counted<V4L2TrackSource>(std::move(capture));
    obj->StartTrack();
    return obj;
}

V4L2TrackSource::V4L2TrackSource(std::shared_ptr<V4L2Capture> capture)
    : capture_(capture),
      width_(capture->width()),
      height_(capture->height()),
      config_width_(capture->width()),
      config_height_(capture->height()),
      src_video_type_(capture->type()) {}

V4L2TrackSource::~V4L2TrackSource() {
    // todo: tell capture unsubscribe observer.
}

void V4L2TrackSource::StartTrack() {
    Init();

    auto observer = capture_->AsObservable();
    observer->Subscribe([this](Buffer buffer) {
        OnFrameCaptured(buffer);
    });
}

void V4L2TrackSource::OnFrameCaptured(Buffer buffer) {
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> dst_buffer = nullptr;
    rtc::TimestampAligner timestamp_aligner_;
    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
        timestamp_aligner_.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
    if (!AdaptFrame(width_, height_, timestamp_us, &adapted_width, &adapted_height,
                    &crop_width, &crop_height, &crop_x, &crop_y)) {
        return;
    }

    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer(webrtc::I420Buffer::Create(width_, height_));
    i420_buffer->InitializeData();

    if (libyuv::ConvertToI420((uint8_t *)buffer.start, buffer.length,
                              i420_buffer.get()->MutableDataY(), i420_buffer.get()->StrideY(),
                              i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
                              i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(),
                              0, 0, width_, height_, width_, height_, libyuv::kRotate0,
                              ConvertVideoType(src_video_type_)) < 0) {
        // "ConvertToI420 Failed"
    }

    dst_buffer = i420_buffer;

    if (adapted_width != width_ || adapted_height != height_) {
        int dst_stride = std::ceil((double)adapted_width / kBufferAlignment) * kBufferAlignment;
        i420_buffer = webrtc::I420Buffer::Create(adapted_width, adapted_height,
                                                 dst_stride, dst_stride/2, dst_stride/2);
        i420_buffer->ScaleFrom(*dst_buffer->ToI420());
        dst_buffer = i420_buffer;
    }

    OnFrame(webrtc::VideoFrame::Builder()
            .set_video_frame_buffer(dst_buffer)
            .set_rotation(webrtc::kVideoRotation_0)
            .set_timestamp_us(translated_timestamp_us)
            .build());
}

webrtc::MediaSourceInterface::SourceState V4L2TrackSource::state() const {
    return SourceState::kLive;
}

bool V4L2TrackSource::remote() const {
    return false;
}

bool V4L2TrackSource::is_screencast() const {
    return false;
}

absl::optional<bool> V4L2TrackSource::needs_denoising() const {
    return false;
}
