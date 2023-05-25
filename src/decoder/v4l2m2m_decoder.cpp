#include "decoder/v4l2m2m_decoder.h"
#include "encoder/raw_buffer.h"

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <stdio.h>

const char *DECODER_FILE = "/dev/video10";

V4l2m2mDecoder::V4l2m2mDecoder(int32_t width, int32_t height)
    : width_(width),
      height_(height)
{}

V4l2m2mDecoder::~V4l2m2mDecoder()
{
    V4l2m2mRelease();
}

int32_t V4l2m2mDecoder::V4l2m2mConfigure()
{
    fd_ = V4l2Util::OpenDevice(DECODER_FILE);
    if (fd_ < 0)
    {
        perror("Open v4l2m2m decoder failed");
    }

    output_.name = "output";
    capture_.name = "capture";
    output_.width = capture_.width = width_;
    output_.height = capture_.height = height_;

    if (!V4l2Util::InitBuffer(fd_, &output_, &capture_))
    {
        exit(-1);
    }

    if (!V4l2Util::SetFormat(fd_, &output_, V4L2_PIX_FMT_H264))
    {
        exit(-1);
    }

    if (!V4l2Util::SetFormat(fd_, &capture_, V4L2_PIX_FMT_YUV420))
    {
        exit(-1);
    }

    // if (!V4l2Util::AllocateBuffer(fd_, &capture_, capture_.type, 1)
    //     || !V4l2Util::AllocateBuffer(fd_, &output_, output_.type, 1))
    // {
    //     exit(-1);
    // }

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.memory = V4L2_MEMORY_MMAP;
    req.type = capture_.type;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0)
    {
        fprintf(stderr, "fd(%d) request buffer: %s\n", fd_, strerror(errno));
        return false;
    }

    struct v4l2_buffer *inner = &capture_.inner;
    inner->type = capture_.type;
    inner->memory = V4L2_MEMORY_MMAP;
    inner->length = 1;
    inner->index = 0;
    inner->m.planes = &capture_.plane;

    if (ioctl(fd_, VIDIOC_QUERYBUF, inner) < 0)
    {
        fprintf(stderr, "fd(%d) query buffer: %s\n", fd_, strerror(errno));
        return false;
    }
    if(capture_.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    {
        capture_.length = inner->m.planes[0].length;
        capture_.start = mmap(NULL, capture_.length,
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd_, inner->m.planes[0].m.mem_offset);
    }
    else if (capture_.type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
    {
        capture_.length = inner->length;
        capture_.start = mmap(NULL, capture_.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, inner->m.offset);
    }

    /* todo: VIDIOC_QUERYBUF output buffer*/

    // turn on streaming
    V4l2Util::StreamOn(fd_, capture_.type);
    V4l2Util::StreamOn(fd_, output_.type);
    std::cout << "V4l2m2m decoder prepare done" << std::endl;
    printf("[V4l2m2mDecoder]: capture addr %p length %d\n", capture_.start,  capture_.length);

    return 1;
}

Buffer V4l2m2mDecoder::V4l2m2mDecode(const uint8_t *byte, uint32_t length)
{
    /*
        todo
    */
    printf("[V4l2m2mDecoder]: decode %p length %d\n", byte, length);

    struct v4l2_buffer buf = {0};
    struct v4l2_plane out_planes = {0};
    buf.memory = V4L2_MEMORY_MMAP;
    // buf.type = capture_.type; // V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
    buf.index = 0;
    buf.length = 1;
    buf.m.planes = &out_planes;

    // Dequeue the output buffer, read the frame and queue it back.
    // buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    // if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
    // {
    //     perror("[V4l2m2mDecoder] ioctl dequeue output");
    // }

    // memcpy((uint8_t *)output_.start, byte, length);
    // output_.length = length;
    // output_.inner.m.planes[0].bytesused = length;

    // if (ioctl(fd_, VIDIOC_QBUF, &output_.inner) < 0)
    // {
    //     perror("[V4l2m2mDecoder] ioctl equeue output");
    // }

    // Dequeue the capture buffer, write out the encoded frame and queue it back.
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    // memcpy((uint8_t *)capture_.start, byte, length);
    // capture_.length = length;
    // capture_.inner.m.planes[0].bytesused = length;
    printf("[V4l2m2mDecoder]: queue buffer %p length %d\n", capture_.start,  capture_.length);
    
    if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0)
    {
        perror("[V4l2m2mDecoder] ioctl enqueue capture");
        exit(1);
    }
    printf("[V4l2m2mDecoder]: 2 dequeue buffer");
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
    {
        perror("[V4l2m2mDecoder] ioctl dequeue capture");
    }

    struct Buffer buffer = {.start = capture_.start,
                            .length = buf.m.planes[0].bytesused,
                            .flags = buf.flags};
    printf("[V4l2m2mDecoder]: %p capture %d, used %d\n", &(buffer.start), buffer.length, buf.m.planes[0].bytesused);

    return buffer;
}

void V4l2m2mDecoder::V4l2m2mRelease()
{
    V4l2Util::StreamOff(fd_, output_.type);
    V4l2Util::StreamOff(fd_, capture_.type);

    munmap(output_.start, output_.length);
    munmap(capture_.start, capture_.length);

    V4l2Util::CloseDevice(fd_);
    printf("[V4l2m2mDecoder]: fd(%d) is released\n", fd_);
}