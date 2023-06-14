#include "v4l2_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

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
    int fd = open(file, O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "v4l2 open(%s): %s\n", file, strerror(errno));
        exit(-1);
    }
    printf("Open file %s fd(%d) success!\n", file, fd);
    return fd;
}

void V4l2Util::CloseDevice(int fd)
{
    close(fd);
}

bool V4l2Util::QueryCapabilities(int fd, v4l2_capability *cap)
{
    if (ioctl(fd, VIDIOC_QUERYCAP, cap) < 0)
    {
        fprintf(stderr, "fd(%d) query capabilities: %s\n", fd, strerror(errno));
        return false;
    }
    return true;
}

bool V4l2Util::InitBuffer(int fd, BufferGroup *output, BufferGroup *capture)
{
    struct v4l2_capability cap = {0};

    if (!V4l2Util::QueryCapabilities(fd, &cap))
    {
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

bool V4l2Util::DequeueBuffer(int fd, v4l2_buffer *buffer)
{
    if (ioctl(fd, VIDIOC_DQBUF, buffer) < 0)
    {
        fprintf(stderr, "fd(%d) dequeue buffer: %s\n", fd, strerror(errno));
        return false;
    }
    return true;
}

bool V4l2Util::QueueBuffer(int fd, v4l2_buffer *buffer)
{
    if (ioctl(fd, VIDIOC_QBUF, buffer) < 0)
    {
        fprintf(stderr, "fd(%d) queue buffer: %s\n", fd, strerror(errno));
        return false;
    }
    return true;
}

std::unordered_set<std::string> V4l2Util::GetDeviceSupportedFormats(const char *file)
{
    int fd = V4l2Util::OpenDevice(file);
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    std::unordered_set<std::string> formats;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
    {
        auto pixel_format = V4l2Util::FourccToString(fmtdesc.pixelformat);
        formats.insert(pixel_format);
        fmtdesc.index++;
    }
    V4l2Util::CloseDevice(fd);

    return formats;
}

bool V4l2Util::SubscribeEvent(int fd, uint32_t type)
{
    struct v4l2_event_subscription sub = {0};
    sub.type = type;
    if (ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0)
    {
        fprintf(stderr, "fd(%d) does not support VIDIOC_SUBSCRIBE_EVENT(%d)\n", fd, type);
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
        fprintf(stderr, "fd(%d) set fps(%d): %s\n", fd, fps, strerror(errno));
        return false;
    }
    return true;
}

bool V4l2Util::SetFormat(int fd, BufferGroup *gbuffer, uint32_t pixel_format)
{
    struct v4l2_format fmt = {0};
    fmt.type = gbuffer->type;
    ioctl(fd, VIDIOC_G_FMT, &fmt);

    printf("V4l2m2m %s formats: %s(%dx%d)", gbuffer->name,
           V4l2Util::FourccToString(fmt.fmt.pix_mp.pixelformat).c_str(),
           fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);

    fmt.fmt.pix_mp.width = gbuffer->width;
    fmt.fmt.pix_mp.height = gbuffer->height;
    fmt.fmt.pix_mp.pixelformat = pixel_format;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        fprintf(stderr, "fd(%d) set format(%s) : %s\n", 
        fd, V4l2Util::FourccToString(fmt.fmt.pix_mp.pixelformat).c_str(),
        strerror(errno));
        return false;
    }

    printf(" -> %s(%dx%d)\n",
           V4l2Util::FourccToString(fmt.fmt.pix_mp.pixelformat).c_str(),
           fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);

    return true;
}

bool V4l2Util::SetCtrl(int fd, unsigned int id, signed int value)
{
    struct v4l2_control ctrls = {0};
    ctrls.id = id;
    ctrls.value = value;
    if (ioctl(fd, VIDIOC_S_CTRL, &ctrls) < 0)
    {
        fprintf(stderr, "fd(%d) set ctrl(%d): %s\n", fd, id, strerror(errno));
        return false;
    }
    return true;
}

bool V4l2Util::SetExtCtrl(int fd, unsigned int id, signed int value)
{
    struct v4l2_ext_controls ctrls = {0};
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
        fprintf(stderr, "fd(%d) set ext ctrl(%d): %s\n", fd, id, strerror(errno));
        return false;
    }
    return true;
}

bool V4l2Util::StreamOn(int fd, v4l2_buf_type type)
{
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0)
    {
        fprintf(stderr, "fd(%d) turn on stream: %s\n", fd, strerror(errno));
        return false;
    }
    return true;
}

bool V4l2Util::StreamOff(int fd, v4l2_buf_type type)
{
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
    {
        fprintf(stderr, "fd(%d) turn off stream: %s\n", fd, strerror(errno));
        return false;
    }
    return true;
}

void V4l2Util::UnMap(struct BufferGroup *gbuffer, int num_buffers)
{
    for (int i = 0; i < num_buffers; i++)
    {
        munmap(gbuffer->buffers[i].start, gbuffer->buffers[i].length);
    }
    fprintf(stderr, "Unmapped (%s) buffers\n", gbuffer->name);
}

bool V4l2Util::MMap(int fd, struct BufferGroup *gbuffer)
{
    for(int i = 0; i < gbuffer->num_buffers; i++)
    {
        Buffer *buffer = &gbuffer->buffers[i];
        struct v4l2_buffer *inner = &buffer->inner;
        inner->type = gbuffer->type;
        inner->memory = V4L2_MEMORY_MMAP;
        inner->length = 1;
        inner->index = i;
        inner->m.planes = &buffer->plane;

        if (ioctl(fd, VIDIOC_QUERYBUF, inner) < 0)
        {
            fprintf(stderr, "fd(%d) query buffer: %s\n", fd, strerror(errno));
            return false;
        }
    
        if(gbuffer->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
            || gbuffer->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
        {
            buffer->length = inner->m.planes[0].length;
            buffer->start = mmap(NULL, buffer->length,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, inner->m.planes[0].m.mem_offset);
        }
        else if (gbuffer->type == V4L2_BUF_TYPE_VIDEO_CAPTURE
            || gbuffer->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
        {
            buffer->length = inner->length;
            buffer->start = mmap(NULL, buffer->length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, inner->m.offset);
        }

        if (MAP_FAILED == buffer->start)
        {
            perror("MAP FAILED");
            munmap(buffer->start, buffer->length);
            return false;
        }

        if (!V4l2Util::QueueBuffer(fd, inner))
        {
            return false;
        }

        printf("V4l2m2m querying %s buffer: %p with %d length\n", 
                gbuffer->name, buffer->start, buffer->length);
    }

    return true;
}

bool V4l2Util::AllocateBuffer(int fd, struct BufferGroup *gbuffer, int num_buffers)
{
    gbuffer->num_buffers = num_buffers;
    gbuffer->buffers = new Buffer[num_buffers];

    struct v4l2_requestbuffers req = {0};
    req.count = num_buffers;
    req.memory = V4L2_MEMORY_MMAP;
    req.type = gbuffer->type;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
        fprintf(stderr, "fd(%d) request buffer: %s\n", fd, strerror(errno));
        return false;
    }

    if (!MMap(fd, gbuffer))
    {
        return false;
    }

    return true;
}
