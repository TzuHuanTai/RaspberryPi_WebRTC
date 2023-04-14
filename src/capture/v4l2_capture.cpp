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
#include <modules/video_capture/video_capture_factory.h>

#include <iostream>

std::shared_ptr<V4L2Capture> V4L2Capture::Create(std::string device)
{
    return std::make_shared<V4L2Capture>(device);
}

V4L2Capture::V4L2Capture(std::string device)
    : device_(device),
      buffer_count_(4),
      rotation_angle_(0)
{
    webrtc::VideoCaptureModule::DeviceInfo *device_info = webrtc::VideoCaptureFactory::CreateDeviceInfo();
    camera_index_ = GetCameraIndex(device_info);
    fd_ = open(device_.c_str(), O_RDWR);
    if (fd_ < 0)
    {
        perror("ioctl open");
    }
}

V4L2Capture::~V4L2Capture()
{
    capture_started = false;
    webrtc::MutexLock lock(&capture_lock_);
    
    if (!capture_thread_.empty())
    {
        capture_thread_.Finalize();
    }

    for (int i = 0; i < buffer_count_; i++)
    {
        munmap(buffers_[i].start, buffers_[i].length);
    }
    delete[] buffers_;

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0)
    {
        perror("VIDIOC_STREAMOFF");
    }

    close(fd_);
    printf("~V4L2Capture fd: %d is closed.\n", fd_);
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
    int fd;
    if ((fd = open(device_.c_str(), O_RDONLY)) != -1)
    {
        // query device capabilities
        struct v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
        {
            if (cap.bus_info[0] != 0 && strcmp((const char *)cap.bus_info, unique_name.c_str()) == 0)
            {
                close(fd);
                return true;
            }
        }
        close(fd);
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
    struct Buffer capture = {
        .name = "v4l2 capture",
        .width = width,
        .height = height,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE
    };

    if (video_type == "i420")
    {
        std::cout << "Use yuv420(i420) format source in v4l2" << std::endl;
        V4l2Util::SetFormat(fd_, &capture, V4L2_PIX_FMT_YUV420);
        capture_video_type_ = webrtc::VideoType::kI420;
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 10000000);
    }
    else if (video_type == "h264")
    {
        std::cout << "Use h264 format source in v4l2" << std::endl;
        V4l2Util::SetFormat(fd_, &capture, V4L2_PIX_FMT_H264);
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
        V4l2Util::SetFormat(fd_, &capture, V4L2_PIX_FMT_MJPEG);
        capture_video_type_ = webrtc::VideoType::kMJPEG;
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 10000000);
    }

    return *this;
}

V4L2Capture &V4L2Capture::SetFps(uint fps)
{
    fps_ = fps;
    struct v4l2_streamparm streamparms = {0};
    streamparms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparms.parm.capture.timeperframe.numerator = 1;
    streamparms.parm.capture.timeperframe.denominator = fps;
    printf("  Fps: %d\n", fps);
    if (ioctl(fd_, VIDIOC_S_PARM, &streamparms) < 0)
    {
        perror("ioctl Setting Fps");
        exit(0);
    }
    return *this;
}

V4L2Capture &V4L2Capture::SetRotation(uint angle)
{
    struct v4l2_control ctrls = {0};
    ctrls.id = V4L2_CID_ROTATE;
    ctrls.value = angle;
    printf("  Rotation: %d\n", angle);
    if (ioctl(fd_, VIDIOC_S_CTRL, &ctrls) < 0)
    {
        perror("ioctl Setting Rotation");
        return *this;
    }
    
    rotation_angle_ = angle;
    return *this;
}

V4L2Capture &V4L2Capture::SetCaptureFunc(std::function<bool()> capture_func)
{
    capture_func_ = std::move(capture_func);
    return *this;
}

bool V4L2Capture::AllocateBuffer()
{
    struct v4l2_requestbuffers req = {0};
    req.count = buffer_count_;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0)
    {
        perror("ioctl Requesting Buffer");
        return false;
    }

    buffers_ = new Buffer[buffer_count_];

    for (int i = 0; i < buffer_count_; i++)
    {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0)
        {
            perror("ioctl Querying Buffer");
            return false;
        }

        buffers_[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
        buffers_[i].length = buf.length;

        if (MAP_FAILED == buffers_[i].start)
        {
            printf("MAP FAILED: %d\n", i);
            for (int j = 0; j < i; j++)
                munmap(buffers_[j].start, buffers_[j].length);
            return false;
        }

        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0)
        {
            perror("ioctl Queue Buffer");
            return false;
        }
    }

    return true;
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
    if (r == 0)
    {
        // timeout
    }

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    while (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
    {
        perror("Retrieving Frame");
    }

    shared_buffer_ = {.start = buffers_[buf.index].start,
                        .length = buf.bytesused,
                        .flags = buf.flags};

    if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0)
    {
        perror("Queue buffer");
    }

    Next(shared_buffer_);
}

Buffer V4L2Capture::GetImage()
{
    return shared_buffer_;
}

void V4L2Capture::StartCapture()
{
    webrtc::MutexLock lock(&capture_lock_);

    if (AllocateBuffer() == false)
    {
        exit(0);
    }

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

    // start camera stream
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0)
    {
        perror("Start Capture");
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
