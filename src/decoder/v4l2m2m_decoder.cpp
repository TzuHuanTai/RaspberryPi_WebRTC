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
{
    std::cout << "[V4l2m2mDecoder]: constructor" << std::endl;

    output_.name = "output";
    capture_.name = "capture";
    output_.width = capture_.width = width;
    output_.height = capture_.height = height;

    V4l2m2mConfigure();
}

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

    if (!V4l2Util::InitBuffer(fd_, &output_, &capture_))
    {
        exit(-1);
    }

    /**
     * requirements
     */
    struct v4l2_event_subscription sub;
    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_SOURCE_CHANGE;
    if (ioctl(fd_, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0)
    {
        perror("the v4l2 driver does not support VIDIOC_SUBSCRIBE_EVENT\n"
               "you must provide codec_height and codec_width on input\n");
    }
    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_EOS;
    if (ioctl(fd_, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0)
    {
        perror("the v4l2 driver does not support end of stream VIDIOC_SUBSCRIBE_EVENT\n");
    }

    if (!V4l2Util::SetFormat(fd_, &output_, V4L2_PIX_FMT_MJPEG))
    {
        exit(-1);
    }

    if (!V4l2Util::SetFormat(fd_, &capture_, V4L2_PIX_FMT_YUV420))
    {
        exit(-1);
    }

    if (!V4l2Util::AllocateBuffer(fd_, &output_) || !V4l2Util::AllocateBuffer(fd_, &capture_))
    {
        exit(-1);
    }

    std::cout << "V4l2m2m all prepare done" << std::endl;

    // turn on streaming
    V4l2Util::SwitchStream(fd_, &output_, true);
    V4l2Util::SwitchStream(fd_, &capture_, true);
    std::cout << "V4l2m2m stream on!" << std::endl;

    return 1;
}

Buffer V4l2m2mDecoder::V4l2m2mDecode(const uint8_t *byte, uint32_t length)
{
    /*
        todo
    */
    printf("[V4l2m2mDecoder]: start decode %p length %d\n", byte, length);

    struct v4l2_buffer buf = {0};
    struct v4l2_plane out_planes = {0};
    buf.memory = V4L2_MEMORY_MMAP;
    buf.length = 1;
    buf.m.planes = &out_planes;

    v4l2_decoder_cmd command = {0};
    command.cmd = V4L2_DEC_CMD_START;
    if (ioctl(fd_, VIDIOC_DECODER_CMD, &command))
    {
        perror("ioctl VIDIOC_DECODER_CMD send start");
        exit(1);
    }

    // Dequeue the output buffer, read the frame and queue it back.
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
    {
        perror("[V4l2m2mDecoder] ioctl dequeue output");
    }

    memcpy((uint8_t *)output_.start, byte, length);
    output_.length = length;
    output_.inner.m.planes[0].bytesused = length;

    if (ioctl(fd_, VIDIOC_QBUF, &output_.inner) < 0)
    {
        perror("[V4l2m2mDecoder] ioctl equeue output");
    }

    command = {0};
    command.cmd = V4L2_DEC_CMD_STOP;
    if (ioctl(fd_, VIDIOC_DECODER_CMD, &command))
    {
        perror("ioctl VIDIOC_DECODER_CMD send end");
        exit(1);
    }

    // Dequeue the capture buffer, write out the encoded frame and queue it back.
    printf("[V4l2m2mDecoder]: start capture\n");
    if (ioctl(fd_, VIDIOC_QBUF, &capture_.inner) < 0)
    {
        perror("[V4l2m2mDecoder] ioctl enqueue capture");
        exit(1);
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
    {
        perror("[V4l2m2mDecoder] ioctl dequeue capture");
    }

    struct Buffer buffer = {.start = capture_.start,
                            .length = capture_.length, // buf.m.planes[0].bytesused,
                            .flags = buf.flags};
    printf("[V4l2m2mDecoder]: %p capture %d, used %d\n", &(buffer.start), buffer.length, buf.m.planes[0].bytesused);

    return buffer;
}

void V4l2m2mDecoder::V4l2m2mRelease()
{
    munmap(output_.start, output_.length);
    munmap(capture_.start, capture_.length);

    // turn off stream
    V4l2Util::SwitchStream(fd_, &output_, false);
    V4l2Util::SwitchStream(fd_, &capture_, false);

    V4l2Util::CloseDevice(fd_);
    printf("[V4l2m2mDecoder]: fd(%d) is released\n", fd_);
}