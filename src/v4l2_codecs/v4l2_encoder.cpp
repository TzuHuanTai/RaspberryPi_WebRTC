#include "v4l2_codecs/v4l2_encoder.h"

#include <sys/mman.h>
#include <iostream>
#include <memory>

const char *ENCODER_FILE = "/dev/video11";
const int BUFFER_NUM = 4;
const int KEY_FRAME_INTERVAL = 12;

V4l2Encoder::V4l2Encoder()
    : framerate_(30),
      bitrate_bps_(10000000) {}

bool V4l2Encoder::Configure(int width, int height, bool is_drm_src) {
    if (!Open(ENCODER_FILE)) {
        return false;
    }

    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE_MODE, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_HEADER_MODE, V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, width * height * 2);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_PROFILE, V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, KEY_FRAME_INTERVAL);

    auto src_memory = is_drm_src? V4L2_MEMORY_DMABUF: V4L2_MEMORY_MMAP;
    PrepareBuffer(&output_, width, height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, src_memory, BUFFER_NUM);
    PrepareBuffer(&capture_, width, height, V4L2_PIX_FMT_H264,
                  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, BUFFER_NUM);

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);

    ResetWorker();

    return true;
}

void V4l2Encoder::SetFps(const webrtc::VideoEncoder::RateControlParameters &parameters,
                         webrtc::BitrateAdjuster &bitrate_adjuster) {
    if (parameters.bitrate.get_sum_bps() <= 0 || parameters.framerate_fps <= 0) {
        return;
    }

    bitrate_adjuster.SetTargetBitrateBps(parameters.bitrate.get_sum_bps());
    uint32_t adjusted_bitrate_bps_ = bitrate_adjuster.GetAdjustedBitrateBps();

    if (adjusted_bitrate_bps_ < 300000) {
        adjusted_bitrate_bps_ = 300000;
    } else {
        adjusted_bitrate_bps_ = (adjusted_bitrate_bps_ / 25000) * 25000;
    }

    if (bitrate_bps_ != adjusted_bitrate_bps_) {
        bitrate_bps_ = adjusted_bitrate_bps_;
        if (!V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, bitrate_bps_)) {
            printf("Encoder failed set bitrate: %d bps\n", bitrate_bps_);
        }
    }

    if (framerate_ != parameters.framerate_fps) {
        framerate_ = parameters.framerate_fps;
        if (!V4l2Util::SetFps(fd_, output_.type, framerate_)) {
            printf("Encoder failed set output fps: %d\n", framerate_);
        }
    }
}
