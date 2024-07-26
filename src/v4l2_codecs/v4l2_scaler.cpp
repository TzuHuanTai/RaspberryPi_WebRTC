#include "v4l2_codecs/v4l2_scaler.h"
#include "common/logging.h"
#include "v4l2_codecs/raw_buffer.h"

const char *SCALER_FILE = "/dev/video12";
const int BUFFER_NUM = 2;

bool V4l2Scaler::Configure(int src_width, int src_height, int dst_width, int dst_height,
                           bool is_drm_src, bool is_drm_dst) {
    if (!Open(SCALER_FILE)) {
        DEBUG_PRINT("Failed to turn on scaler: %s", SCALER_FILE);
        return false;
    }
    auto src_memory = is_drm_src ? V4L2_MEMORY_DMABUF : V4L2_MEMORY_MMAP;
    PrepareBuffer(&output_, src_width, src_height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, src_memory, BUFFER_NUM);
    PrepareBuffer(&capture_, dst_width, dst_height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, BUFFER_NUM, is_drm_dst);

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);

    return true;
}
