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
#include <media/base/video_common.h>
#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_factory.h>
#include <rtc_base/logging.h>
#include <rtc_base/ref_counted_object.h>

#include <iostream>

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
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
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
    struct v4l2_streamparm streamparms;
    ioctl(fd_, VIDIOC_G_PARM, &streamparms);
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
    if (AllocateBuffer() == false)
    {
        exit(0);
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0)
    {
        perror("Start Capture");
    }
}
