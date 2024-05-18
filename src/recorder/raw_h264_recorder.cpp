#include "recorder/raw_h264_recorder.h"

const int SECOND_PER_FILE = 300;

std::unique_ptr<RawH264Recorder> RawH264Recorder::Create(Args config) {
    auto ptr = std::make_unique<RawH264Recorder>(config, "h264_v4l2m2m");
    ptr->Initialize();
    return ptr;
}

RawH264Recorder::RawH264Recorder(Args config, std::string encoder_name)
    : VideoRecorder(config, encoder_name),
      has_first_keyframe_(false) {};

RawH264Recorder::~RawH264Recorder() {}

void RawH264Recorder::Encode(Buffer buffer) {
    if (buffer.flags & V4L2_BUF_FLAG_KEYFRAME && !has_first_keyframe_ ) {
        has_first_keyframe_ = true;
    }

    if (has_first_keyframe_) {
        PushBuffer(buffer);
    }
}
