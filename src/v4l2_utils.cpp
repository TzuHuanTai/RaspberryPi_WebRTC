#include "v4l2_utils.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <memory>

bool V4l2Util::IsSinglePlaneVideo(struct v4l2_capability *cap)
{
    return (cap->capabilities & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT) &&
            (cap->capabilities & V4L2_CAP_STREAMING)) ||
           (cap->capabilities & V4L2_CAP_VIDEO_M2M);
}

bool V4l2Util::IsMultiPlaneVideo(struct v4l2_capability *cap)
{
    return (cap->capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE) &&
            (cap->capabilities & V4L2_CAP_STREAMING)) ||
           (cap->capabilities & V4L2_CAP_VIDEO_M2M_MPLANE);
}

std::string V4l2Util::FourccToString(uint32_t fourcc)
{
    int length = 4;
    std::string buf;
    buf.resize(length);

    for (int i = 0; i < length; i++)
    {
        const int c = fourcc & 0xff;
        buf[i] = c;
        fourcc >>= 8;
    }

    return buf;
}

int V4l2Util::OpenDevice(const char *file)
{
    int fd = open(file, O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        perror("Open v4l2m2m encoder failed");
    }
    return fd;
}

void V4l2Util::CloseDevice(int fd)
{
    close(fd);
}

bool V4l2Util::InitBuffer(int fd, Buffer *output, Buffer *capture)
{
    struct v4l2_capability cap = {{0}};

    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        perror("ioctl capability");
        return false;
    }

    printf("driver '%s' on card '%s' in %s mode\n", cap.driver, cap.card,
           V4l2Util::IsSinglePlaneVideo(&cap) ? "splane" : V4l2Util::IsMultiPlaneVideo(&cap) ? "mplane"
                                                                                             : "unknown");

    if (V4l2Util::IsMultiPlaneVideo(&cap))
    {
        output->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        capture->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }
    else if (V4l2Util::IsSinglePlaneVideo(&cap))
    {
        output->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        capture->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        return false;
    }

    return true;
}

bool V4l2Util::SetFps(int fd, uint32_t type, int fps)
{
    struct v4l2_streamparm streamparms = {0};
    streamparms.type = type;
    streamparms.parm.capture.timeperframe.numerator = 1;
    streamparms.parm.capture.timeperframe.denominator = fps;
    if (ioctl(fd, VIDIOC_S_PARM, &streamparms) < 0)
    {
        perror("ioctl Setting Fps");
        return false;
    }
    return true;
}

bool V4l2Util::SetFormat(int fd, Buffer *buffer, uint32_t pixel_format)
{
    struct v4l2_format fmt = {0};
    fmt.type = buffer->type;
    ioctl(fd, VIDIOC_G_FMT, &fmt);

    printf("V4l2m2m %s formats: %s(%dx%d)", buffer->name,
           V4l2Util::FourccToString(fmt.fmt.pix_mp.pixelformat).c_str(),
           fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);
    fmt.fmt.pix_mp.width = buffer->width;
    fmt.fmt.pix_mp.height = buffer->height;
    fmt.fmt.pix_mp.pixelformat = pixel_format;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        perror("ioctl set format failed");
        return false;
    }

    printf(" -> %s(%dx%d)\n",
           V4l2Util::FourccToString(fmt.fmt.pix_mp.pixelformat).c_str(),
           fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);

    return true;
}

bool V4l2Util::SetExtCtrl(int fd, unsigned int id, signed int value)
{
    struct v4l2_ext_controls ctrls = {{0}};
    struct v4l2_ext_control ctrl = {0};

    /* set ctrls */
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CODEC;
    ctrls.controls = &ctrl;
    ctrls.count = 1;

    /* set ctrl*/
    ctrl.id = id;
    ctrl.value = value;

    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
    {
        printf("Failed to set %d ext ctrls %d.\n", id, value);
        return false;
    }
    return true;
}

bool V4l2Util::SwitchStream(int fd, Buffer *buffer, bool onoff)
{
    if (onoff && ioctl(fd, VIDIOC_STREAMON, &buffer->type) < 0)
    {
        perror("Turn off v4l2m2m encoder capture stream failed");
        return false;
    }

    if (!onoff && ioctl(fd, VIDIOC_STREAMOFF, &buffer->type) < 0)
    {
        perror("Turn off v4l2m2m encoder capture stream failed");
        return false;
    }
    return true;
}

bool V4l2Util::MMap(int fd, struct Buffer *buffer)
{
    struct v4l2_buffer *inner = &buffer->inner;
    inner->type = buffer->type;
    inner->memory = V4L2_MEMORY_MMAP;
    inner->length = 1;
    inner->index = 0;
    inner->m.planes = &buffer->plane;

    if (ioctl(fd, VIDIOC_QUERYBUF, inner) < 0)
    {
        perror("ioctl Querying Buffer");
        return false;
    }

    buffer->length = inner->m.planes[0].length;
    buffer->start = mmap(NULL, buffer->length,
                         PROT_READ | PROT_WRITE, MAP_SHARED, fd, inner->m.planes[0].m.mem_offset);

    if (MAP_FAILED == buffer->start)
    {
        perror("MAP FAILED");
        munmap(buffer->start, buffer->length);
        return false;
    }

    printf("V4l2m2m querying %s buffer: %p with %d length\n", buffer->name, &(buffer->start), buffer->length);

    return true;
}

bool V4l2Util::AllocateBuffer(int fd, struct Buffer *buffer)
{
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.memory = V4L2_MEMORY_MMAP;
    req.type = buffer->type;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
        perror("ioctl Requesting Buffer");
        return false;
    }
    if (!MMap(fd, buffer))
    {
        return false;
    }

    return true;
}
