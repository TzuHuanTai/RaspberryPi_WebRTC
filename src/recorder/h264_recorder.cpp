#include "recorder/h264_recorder.h"

std::unique_ptr<H264Recorder> H264Recorder::Create(std::shared_ptr<V4L2Capture> capture) {
    auto ptr = std::make_unique<H264Recorder>(capture, "h264_v4l2m2m");
    ptr->Initialize();
    ptr->AsyncWriteFileBackground();
    return std::move(ptr);
}
