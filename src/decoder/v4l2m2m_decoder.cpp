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

V4l2m2mDecoder::V4l2m2mDecoder()
    : buffer_count_(1) {}

V4l2m2mDecoder::~V4l2m2mDecoder() {
    V4l2m2mRelease();
}

bool V4l2m2mDecoder::V4l2m2mConfigure(int width, int height) {
    fd_ = V4l2Util::OpenDevice(DECODER_FILE);
    if (fd_ < 0) {
        return false;
    }

    if (!V4l2Util::InitBuffer(fd_, &output_, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP) 
        || !V4l2Util::InitBuffer(fd_, &capture_, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP)) {
        return false;
    }

    if (!V4l2Util::SetFormat(fd_, &output_, width, height, V4L2_PIX_FMT_H264)) {
        return false;
    }

    if (!V4l2Util::SetFormat(fd_, &capture_, width, height, V4L2_PIX_FMT_YUV420)) {
        return false;
    }

    V4l2Util::SubscribeEvent(fd_, V4L2_EVENT_SOURCE_CHANGE);
    V4l2Util::SubscribeEvent(fd_, V4L2_EVENT_EOS);

    if (!V4l2Util::AllocateBuffer(fd_, &output_, buffer_count_)
        || !V4l2Util::AllocateBuffer(fd_, &capture_, buffer_count_)) {
        return false;
    }

    if (!V4l2Util::QueueBuffers(fd_, &output_)
        || !V4l2Util::QueueBuffers(fd_, &capture_)) {
        return false;
    }

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);

    std::cout << "[V4l2m2mDecoder]: prepare done" << std::endl;
    return true;
}

bool V4l2m2mDecoder::V4l2m2mDecode(const uint8_t *byte, uint32_t length, Buffer &buffer) {
    struct v4l2_buffer buf = {};
    struct v4l2_plane out_planes = {};
    buf.memory = V4L2_MEMORY_MMAP;
    buf.length = 1;
    buf.m.planes = &out_planes;

    while (true) {
        fd_set fds[3];
        fd_set *rd_fds = &fds[0]; /* for capture */
        fd_set *wr_fds = &fds[1]; /* for output */
        fd_set *ex_fds = &fds[2]; /* for handle event */
        FD_ZERO(rd_fds);
        FD_SET(fd_, rd_fds);
        FD_ZERO(ex_fds);
        FD_SET(fd_, ex_fds);
        FD_ZERO(wr_fds);
        FD_SET(fd_, wr_fds);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int r = select(fd_ + 1, rd_fds, wr_fds, ex_fds, &tv);

        if (r <= 0) { // timeout or failed
            return false;
        }

        if (rd_fds && FD_ISSET(fd_, rd_fds)) {
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            if (!V4l2Util::DequeueBuffer(fd_, &buf)) {
                return false;
            }

            buffer.start = capture_.buffers[buf.index].start;
            buffer.length = buf.m.planes[0].bytesused;
            buffer.flags = buf.flags;

            if (!V4l2Util::QueueBuffer(fd_, &capture_.buffers[buf.index].inner)) {
                return false;
            }
        }

        if (wr_fds && FD_ISSET(fd_, wr_fds)) {
            buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            if (!V4l2Util::DequeueBuffer(fd_, &buf)) {
                return false;
            }

            memcpy((uint8_t *)output_.buffers[buf.index].start, byte, length);

            if (!V4l2Util::QueueBuffer(fd_, &output_.buffers[buf.index].inner)) {
                return false;
            }
            break;
        }

        if (ex_fds && FD_ISSET(fd_, ex_fds)) {
            fprintf(stderr, "Exception in decoder.\n");
            HandleEvent();
            break;
        }
        /* EAGAIN - continue select loop. */
    }

    return true;
}

void V4l2m2mDecoder::V4l2m2mRelease() {
    V4l2Util::StreamOff(fd_, output_.type);
    V4l2Util::StreamOff(fd_, capture_.type);

    V4l2Util::DeallocateBuffer(fd_, &output_);
    V4l2Util::DeallocateBuffer(fd_, &capture_);

    V4l2Util::CloseDevice(fd_);
}

void V4l2m2mDecoder::HandleEvent() {
    struct v4l2_event ev;
    while (!ioctl(fd_, VIDIOC_DQEVENT, &ev)) {
        switch (ev.type) {
        case V4L2_EVENT_SOURCE_CHANGE:
            printf("[V4l2m2mDecoder]: Source changed!\n");
            V4l2Util::StreamOff(fd_, capture_.type);
            V4l2Util::DeallocateBuffer(fd_, &capture_);
            // todo: VIDIOC_G_FMT get new resolution then reset capture format
            V4l2Util::AllocateBuffer(fd_, &capture_, buffer_count_);
            V4l2Util::StreamOn(fd_, capture_.type);
            break;
        case V4L2_EVENT_EOS:
            printf("[V4l2m2mDecoder]: EOS!\n");
            break;
        }
    }
}
