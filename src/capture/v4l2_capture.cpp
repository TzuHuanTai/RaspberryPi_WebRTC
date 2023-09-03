#include "v4l2_capture.h"

// Linux
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/select.h>

// WebRTC
#include <modules/video_capture/video_capture_factory.h>

#include <iostream>

std::shared_ptr<V4L2Capture> V4L2Capture::Create(std::string device)
{
    return std::make_shared<V4L2Capture>(device);
}

V4L2Capture::V4L2Capture(std::string device)
    : buffer_count_(2),
      capture_started(false)
{
    webrtc::VideoCaptureModule::DeviceInfo *device_info = webrtc::VideoCaptureFactory::CreateDeviceInfo();
    fd_ = V4l2Util::OpenDevice(device.c_str());
    camera_index_ = GetCameraIndex(device_info);

    capture_.name = "v4l2_capture";
    capture_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

V4L2Capture::~V4L2Capture()
{
    capture_started = false;
    webrtc::MutexLock lock(&capture_lock_);
    
    if (!capture_thread_.empty())
    {
        capture_thread_.Finalize();
    }

    for (int i = 0; i < capture_.num_buffers; i++)
    {
        munmap(capture_.buffers[i].start, capture_.buffers[i].length);
    }

    V4l2Util::StreamOff(fd_, capture_.type);

    V4l2Util::CloseDevice(fd_);
    printf("~V4L2Capture fd(%d) is closed.\n", fd_);
}

void V4L2Capture::Next(Buffer buffer)
{
    for (auto observer : observers_)
    {
        if (observer->subscribed_func_ != nullptr)
        {
            observer->subscribed_func_(buffer);
        }
    }
}

std::shared_ptr<Observable<Buffer>> V4L2Capture::AsObservable()
{
    auto observer = std::make_shared<Observable<Buffer>>();
    observers_.push_back(observer);
    return observer;
}

void V4L2Capture::UnSubscribe()
{
    auto it = observers_.begin();
    while (it != observers_.end())
    {
        it->reset();
        it = observers_.erase(it);
    }
}

bool V4L2Capture::CheckMatchingDevice(std::string unique_name)
{
    struct v4l2_capability cap;
    if (V4l2Util::QueryCapabilities(fd_, &cap)
        && cap.bus_info[0] != 0
        && strcmp((const char *)cap.bus_info, unique_name.c_str()) == 0)
    {
        return true;
    }
    return false;
}

int V4L2Capture::GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info)
{
    for (int i = 0; i < device_info->NumberOfDevices(); i++)
    {
        char device_name[256];
        char unique_name[256];
        if (device_info->GetDeviceName(static_cast<uint32_t>(i), device_name,
                                       sizeof(device_name), unique_name,
                                       sizeof(unique_name)) == 0 &&
            CheckMatchingDevice(unique_name))
        {
            std::cout << "GetDeviceName(" << i
                      << "): device_name=" << device_name
                      << ", unique_name=" << unique_name << std::endl;
            return i;
        }
    }
    return -1;
}

V4L2Capture &V4L2Capture::SetFormat(uint width, uint height, std::string video_type)
{
    width_ = width;
    height_ = height;
    capture_.width = width;
    capture_.height = height;

    if (video_type == "i420")
    {
        std::cout << "Use yuv420(i420) format source in v4l2" << std::endl;
        V4l2Util::SetFormat(fd_, &capture_, V4L2_PIX_FMT_YUV420);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 10000000);
        capture_video_type_ = webrtc::VideoType::kI420;
    }
    else if (video_type == "h264")
    {
        std::cout << "Use h264 format source in v4l2" << std::endl;
        V4l2Util::SetFormat(fd_, &capture_, V4L2_PIX_FMT_H264);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE_MODE, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_PROFILE, V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_3_1);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, 12);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 2000000);
        capture_video_type_ = webrtc::VideoType::kUnknown;
    }
    else {
        std::cout << "Use mjpeg format source in v4l2" << std::endl;
        V4l2Util::SetFormat(fd_, &capture_, V4L2_PIX_FMT_MJPEG);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 10000000);
        capture_video_type_ = webrtc::VideoType::kMJPEG;
    }

    return *this;
}

V4L2Capture &V4L2Capture::SetFps(uint fps)
{
    fps_ = fps;
    printf("  Fps: %d\n", fps);

    if (!V4l2Util::SetFps(fd_, capture_.type, fps))
    {
        exit(0);
    }

    return *this;
}

V4L2Capture &V4L2Capture::SetRotation(uint angle)
{
    printf("  Rotation: %d\n", angle);
    V4l2Util::SetCtrl(fd_, V4L2_CID_ROTATE, angle);

    return *this;
}

V4L2Capture &V4L2Capture::SetCaptureFunc(std::function<bool()> capture_func)
{
    capture_func_ = std::move(capture_func);
    return *this;
}

void V4L2Capture::CaptureImage()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int r = select(fd_ + 1, &fds, NULL, NULL, &tv);
    if (r <= 0) // timeout or failed
    {
        return;
    }

    struct v4l2_buffer buf = {0};
    buf.type = capture_.type;
    buf.memory = V4L2_MEMORY_MMAP;

    V4l2Util::DequeueBuffer(fd_, &buf);

    shared_buffer_ = {.start = capture_.buffers[buf.index].start,
                        .length = buf.bytesused,
                        .flags = buf.flags};

    V4l2Util::QueueBuffer(fd_, &buf);

    Next(shared_buffer_);
}

Buffer V4L2Capture::GetImage()
{
    return shared_buffer_;
}

void V4L2Capture::StartCapture()
{
    webrtc::MutexLock lock(&capture_lock_);

    if (!V4l2Util::AllocateBuffer(fd_, &capture_, buffer_count_))
    {
        exit(0);
    }

    V4l2Util::StreamOn(fd_, capture_.type);

    if (capture_func_ == nullptr)
    {
        capture_func_ = [this]() -> bool
        { return CaptureProcess(); };
    }

    // start capture thread;
    if (capture_thread_.empty())
    {
        capture_thread_ = rtc::PlatformThread::SpawnJoinable(
            [this]()
            { this->CaptureThread(); },
            "CaptureThread",
            rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));
    }

    capture_started = true;
}

void V4L2Capture::CaptureThread()
{
    while (capture_func_()) { }
}

bool V4L2Capture::CaptureProcess()
{
    webrtc::MutexLock lock(&capture_lock_);
    if (capture_started)
    {
        CaptureImage();
        return true;
    }
    return false;
}
