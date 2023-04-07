#include "v4l2_capture.h"

// Linux
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

// WebRTC
#include <api/scoped_refptr.h>
#include <api/video/i420_buffer.h>
#include <api/video/video_frame_buffer.h>
#include <media/base/video_common.h>
#include <media/base/video_adapter.h>
#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_factory.h>
#include <rtc_base/logging.h>
#include <rtc_base/ref_counted_object.h>
#include <rtc_base/timestamp_aligner.h>
#include <third_party/libyuv/include/libyuv.h>

#include <iostream>

rtc::scoped_refptr<V4L2TrackSource> V4L2TrackSource::Create(
    std::shared_ptr<V4L2Capture> capture)
{
    return rtc::make_ref_counted<V4L2TrackSource>(capture);
}

V4L2TrackSource::V4L2TrackSource(
    std::shared_ptr<V4L2Capture> capture)
    : fps_(capture->fps_),
      width_(capture->width_),
      height_(capture->height_),
      capture_video_type_(capture->capture_video_type_)
{ 
    // switch(video_type)
    // {
    // case "i420":
    //     capture_video_type_ = webrtc::VideoType::kI420;
    //     break;
    // case "mjpeg":
    //     capture_video_type_ = webrtc::VideoType::kMJPEG;
    //     break;
    // case "h264":
    //     capture_video_type_ = webrtc::VideoType::kUnknown;
    //     break;
    // default:
    //     capture_video_type_ = webrtc::VideoType::kUnknown;
    //     break;
    // }
}

V4L2TrackSource::~V4L2TrackSource()
{
    webrtc::MutexLock lock(&capture_lock_);
    if (!capture_thread_.empty())
    {
        capture_thread_.Finalize();
    }

    capture_started = false;
}

V4L2Capture &V4L2Capture::SetCaptureFunc(std::function<bool()> capture_func)
{
    capture_func_ = std::move(capture_func);
    return *this;
}

void V4L2Capture::StartTrack()
{
    webrtc::MutexLock lock(&capture_lock_);

    if (capture_func_ == nullptr)
    {
        capture_func_ = [this]() -> bool
        { return TrackProcess(); };
    }

    // start capture thread;
    if (capture_thread_.empty())
    {
        capture_thread_ = rtc::PlatformThread::SpawnJoinable(
            [this]()
            { this->TrackThread(); },
            "TrackThread",
            rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));
    }

    capture_started = true;
}

void V4L2Capture::TrackThread(/*todo: pass shared buffer & fps*/)
{
    std::cout << "TrackThread: start" << std::endl;

    while (capture_started)
    {
    }

    capture_started = false;
}

bool V4L2Capture::TrackProcess()
{
    webrtc::MutexLock lock(&capture_lock_);
    if (!capture_started)
    {
        return false;
    }

    if (capture_started)
    {
        OnFrameCaptured(*(capture->shared_buffers_));
    }
    usleep(0);
    return true;
}

void V4L2Capture::OnFrameCaptured(Buffer buffer)
{
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> dst_buffer = nullptr;
    rtc::TimestampAligner timestamp_aligner_;
    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
        timestamp_aligner_.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
    if (!AdaptFrame(width_, height_, timestamp_us, &adapted_width, &adapted_height,
                    &crop_width, &crop_height, &crop_x, &crop_y))
    {
        return;
    }

    if (capture_video_type_ == webrtc::VideoType::kUnknown) {
        rtc::scoped_refptr<RawBuffer> raw_buffer(
            RawBuffer::Create(adapted_width, adapted_height, buffer.length));
        std::memcpy(raw_buffer->MutableData(),
                    (uint8_t *)buffer.start,
                    buffer.length);
        dst_buffer = raw_buffer;
    }
    else
    {
        rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer(webrtc::I420Buffer::Create(width_, height_));
        i420_buffer->InitializeData();

        if (libyuv::ConvertToI420((uint8_t *)buffer.start, buffer.length,
                                i420_buffer.get()->MutableDataY(), i420_buffer.get()->StrideY(),
                                i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
                                i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(),
                                0, 0, width_, height_, width_, height_, libyuv::kRotate0,
                                ConvertVideoType(capture_video_type_)) < 0)
        {
            RTC_LOG(LS_ERROR) << "ConvertToI420 Failed";
        }
        else
        {
            dst_buffer = i420_buffer;
        }

        if (adapted_width != width_ || adapted_height != height_)
        {
            i420_buffer = webrtc::I420Buffer::Create(adapted_width, adapted_height);
            i420_buffer->ScaleFrom(*dst_buffer->ToI420());
            dst_buffer = i420_buffer;
        }
    }
    
    OnFrame(webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(dst_buffer)
                .set_rotation(webrtc::kVideoRotation_0)
                .set_timestamp_us(translated_timestamp_us)
                .build());
}

webrtc::MediaSourceInterface::SourceState V4L2Capture::state() const
{
    return SourceState::kLive;
}

bool V4L2Capture::remote() const
{
    return false;
}

bool V4L2Capture::is_screencast() const
{
    return false;
}

absl::optional<bool> V4L2Capture::needs_denoising() const
{
    return false;
}
