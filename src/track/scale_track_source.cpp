#include "track/scale_track_source.h"

#include <cmath>

// WebRTC
#include <api/video/i420_buffer.h>
#include <api/video/video_frame_buffer.h>
#include <third_party/libyuv/include/libyuv.h>

#include "common/logging.h"

static const int kBufferAlignment = 64;

rtc::scoped_refptr<ScaleTrackSource>
ScaleTrackSource::Create(std::shared_ptr<V4L2Capture> capture) {
    auto obj = rtc::make_ref_counted<ScaleTrackSource>(std::move(capture));
    obj->StartTrack();
    return obj;
}

ScaleTrackSource::ScaleTrackSource(std::shared_ptr<V4L2Capture> capture)
    : capture(capture),
      width(capture->width()),
      height(capture->height()),
      src_video_type(capture->type()) {}

ScaleTrackSource::~ScaleTrackSource() {
    // todo: tell capture unsubscribe observer.
}

void ScaleTrackSource::StartTrack() {
    auto observer = capture->AsObservable();
    observer->Subscribe([this](rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) {
        OnFrameCaptured(frame_buffer);
    });
}

void ScaleTrackSource::OnFrameCaptured(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) {
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> dst_buffer = nullptr;

    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
        timestamp_aligner.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
    if (!AdaptFrame(width, height, timestamp_us, &adapted_width, &adapted_height, &crop_width,
                    &crop_height, &crop_x, &crop_y)) {
        return;
    }

    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer(webrtc::I420Buffer::Create(width, height));
    i420_buffer->InitializeData();

    if (libyuv::ConvertToI420((uint8_t *)frame_buffer->Data(), frame_buffer->size(),
                              i420_buffer.get()->MutableDataY(), i420_buffer.get()->StrideY(),
                              i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
                              i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(), 0, 0,
                              width, height, width, height, libyuv::kRotate0,
                              ConvertVideoType(src_video_type)) < 0) {
        ERROR_PRINT("ConvertToI420 Failed");
    }

    i420_raw_buffer = i420_buffer;
    dst_buffer = i420_buffer;

    if (adapted_width != width || adapted_height != height) {
        int dst_stride = std::ceil((double)adapted_width / kBufferAlignment) * kBufferAlignment;
        i420_buffer = webrtc::I420Buffer::Create(adapted_width, adapted_height, dst_stride,
                                                 dst_stride / 2, dst_stride / 2);
        i420_buffer->ScaleFrom(*dst_buffer->ToI420());
        dst_buffer = i420_buffer;
    }

    OnFrame(webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(dst_buffer)
                .set_rotation(webrtc::kVideoRotation_0)
                .set_timestamp_us(translated_timestamp_us)
                .build());
}

rtc::scoped_refptr<webrtc::I420BufferInterface> ScaleTrackSource::GetI420Frame() {
    return i420_raw_buffer->ToI420();
}

webrtc::MediaSourceInterface::SourceState ScaleTrackSource::state() const {
    return SourceState::kLive;
}

bool ScaleTrackSource::remote() const { return false; }

bool ScaleTrackSource::is_screencast() const { return false; }

absl::optional<bool> ScaleTrackSource::needs_denoising() const { return false; }
