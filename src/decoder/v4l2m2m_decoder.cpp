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
const int BUFFER_NUM = 2;

bool V4l2m2mDecoder::V4l2m2mConfigure(int width, int height, bool is_drm_dst) {
    if(!Open(DECODER_FILE)) {
        printf("failed to open scaler: /dev/video10\n");
    }

    PrepareBuffer(&output_, width, height, V4L2_PIX_FMT_H264,
                  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, BUFFER_NUM);
    PrepareBuffer(&capture_, width, height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, BUFFER_NUM, is_drm_dst);

    V4l2Util::SubscribeEvent(fd_, V4L2_EVENT_SOURCE_CHANGE);
    V4l2Util::SubscribeEvent(fd_, V4L2_EVENT_EOS);

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);

    ResetWorker();

    std::cout << "[V4l2m2mDecoder]: prepare done" << std::endl;
    return true;
}

void V4l2m2mDecoder::HandleEvent() {
    struct v4l2_event ev;
    while (!ioctl(fd_, VIDIOC_DQEVENT, &ev)) {
        switch (ev.type) {
        case V4L2_EVENT_SOURCE_CHANGE:
            printf("[V4l2m2mDecoder]: Source changed!\n");
            V4l2Util::StreamOff(fd_, capture_.type);
            V4l2Util::DeallocateBuffer(fd_, &capture_);
            V4l2Util::SetFormat(fd_, &capture_, 0 ,0 ,0);
            V4l2Util::AllocateBuffer(fd_, &capture_, BUFFER_NUM);
            V4l2Util::StreamOn(fd_, capture_.type);
            break;
        case V4L2_EVENT_EOS:
            printf("[V4l2m2mDecoder]: EOS!\n");
            break;
        }
    }
}
