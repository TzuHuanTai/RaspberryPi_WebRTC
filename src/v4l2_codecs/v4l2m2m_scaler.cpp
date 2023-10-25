#include "v4l2_codecs/v4l2m2m_scaler.h"
#include "v4l2_codecs/raw_buffer.h"

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

const char *SCALER_FILE = "/dev/video12";
const int BUFFER_NUM = 2;

bool V4l2m2mScaler::V4l2m2mConfigure(int src_width, int src_height, 
                                     int dst_width, int dst_height,
                                     bool is_drm_src, bool is_drm_dst) {
    if(!Open(SCALER_FILE)) {
        printf("failed to open scaler: /dev/video12\n");
    }
    auto src_memory = is_drm_src? V4L2_MEMORY_DMABUF: V4L2_MEMORY_MMAP;
    PrepareBuffer(&output_, src_width, src_height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, src_memory, BUFFER_NUM);
    PrepareBuffer(&capture_, dst_width, dst_height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, BUFFER_NUM, is_drm_dst);

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);

    ResetWorker();

    std::cout << "[V4l2m2mScaler]: prepare done" << std::endl;
    return true;
}
