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

rtc::scoped_refptr<V4L2Capture> V4L2Capture::Create(std::string device)
{
    auto capture = rtc::make_ref_counted<V4L2Capture>(device);
    return capture;
}

V4L2Capture::V4L2Capture(std::string device) : device_(device)
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

    exit_ = true;

    // turn off stream
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0)
    {
        RTC_LOG(LS_INFO) << "VIDIOC_STREAMOFF error. errno: " << errno;
    }

    printf("~V4L2Capture Close fd: %d\n", fd_);
    close(fd_);
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

V4L2Capture &V4L2Capture::SetFormat(uint width, uint height)
{
    width_ = width;
    height_ = height;
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.sizeimage = 0;
    printf("  Width: %d\n"
           "  Height: %d\n",
           width, height);

    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0)
    {
        perror("ioctl Setting Pixel Format");
        exit(0);
    }
    return *this;
}

V4L2Capture &V4L2Capture::SetFps(uint fps)
{
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
        printf("Index: %d\n", i);
        printf("Length: %u\nAddress: %p\n", buffers_[i].length, buffers_[i].start);
        printf("Image Length: %d\n", buf.bytesused);

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

Buffer V4L2Capture::CaptureImage()
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

    struct Buffer buffer = {.start = buffers_[buf.index].start,
                            .length = buf.bytesused};

    if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0)
    {
        perror("Queue buffer");
    }

    return buffer;
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

    captureStarted_ = true;
    std::cout << "StartCapture: complete!" << std::endl;
}

void V4L2Capture::CaptureThread()
{
    std::cout << "CaptureThread: start" << std::endl;

    while (capture_func_())
    {
    }
}

bool V4L2Capture::CaptureProcess()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int res = select(fd_ + 1, &fds, NULL, NULL, &tv);
    {
        webrtc::MutexLock lock(&capture_lock_);
        if (exit_)
        {
            return false;
        }
        else if (res == 0)
        {
            return true;
        }

        if (captureStarted_)
        {
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            while (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
            {
                perror("Retrieving Frame");
            }

            OnFrameCaptured((uint8_t *)buffers_[buf.index].start, buf.bytesused);

            if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0)
            {
                perror("Queue buffer");
            }
        }
    }
    usleep(0);
    return true;
}

void V4L2Capture::OnFrameCaptured(uint8_t *data, uint32_t bytesused)
{
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> dst_buffer = nullptr;

    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer(
        webrtc::I420Buffer::Create(width_, height_));
    i420_buffer->InitializeData();
    if (libyuv::ConvertToI420(
            data, bytesused, i420_buffer.get()->MutableDataY(),
            i420_buffer.get()->StrideY(), i420_buffer.get()->MutableDataU(),
            i420_buffer.get()->StrideU(), i420_buffer.get()->MutableDataV(),
            i420_buffer.get()->StrideV(), 0, 0, width_, height_,
            width_, height_, libyuv::kRotate0,
            ConvertVideoType(capture_viedo_type_)) < 0)
    {
        RTC_LOG(LS_ERROR) << "ConvertToI420 Failed";
    }
    else
    {
        dst_buffer = i420_buffer;
    }

    if (dst_buffer)
    {
        webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(dst_buffer)
                                       .set_timestamp_rtp(0)
                                       .set_timestamp_ms(rtc::TimeMillis())
                                       .set_timestamp_us(rtc::TimeMicros())
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build();

        rtc::TimestampAligner timestamp_aligner_;
        const int64_t timestamp_us = frame.timestamp_us();
        const int64_t translated_timestamp_us =
            timestamp_aligner_.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

        int adapted_width;
        int adapted_height;
        int crop_width;
        int crop_height;
        int crop_x;
        int crop_y;
        if (!AdaptFrame(frame.width(), frame.height(), timestamp_us, &adapted_width,
                        &adapted_height, &crop_width, &crop_height, &crop_x,
                        &crop_y))
        {
            return;
        }

        rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer =
            frame.video_frame_buffer();

        if (adapted_width != frame.width() || adapted_height != frame.height())
        {
            // Video adapter has requested a down-scale. Allocate a new buffer and
            // return scaled version.
            rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer =
                webrtc::I420Buffer::Create(adapted_width, adapted_height);
            i420_buffer->ScaleFrom(*buffer->ToI420());
            buffer = i420_buffer;
        }

        OnFrame(webrtc::VideoFrame::Builder()
                    .set_video_frame_buffer(buffer)
                    .set_rotation(frame.rotation())
                    .set_timestamp_us(translated_timestamp_us)
                    .build());
    }
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
