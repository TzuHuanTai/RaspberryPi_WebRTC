#include "recorder/raw_h264_recorder.h"

const int SECOND_PER_FILE = 300;

std::unique_ptr<RawH264Recorder> RawH264Recorder::Create(std::shared_ptr<V4L2Capture> capture) {
    auto ptr = std::make_unique<RawH264Recorder>(capture, "h264_v4l2m2m");
    ptr->SubscribeBufferSource();
    return std::move(ptr);
}

RawH264Recorder::RawH264Recorder(std::shared_ptr<V4L2Capture> capture, std::string encoder_name)
    : VideoRecorder(capture, encoder_name),
      frame_count_(0),
      has_first_keyframe_(false) {};

RawH264Recorder::~RawH264Recorder() {}

void RawH264Recorder::RecordingLoop() {
    if(capture->format() != V4L2_PIX_FMT_H264 || abort || raw_buffer_queue.empty()) {
        usleep(1000);
        return;
    }

    auto buffer = raw_buffer_queue.front();
    raw_buffer_queue.pop();

    if (buffer.flags & V4L2_BUF_FLAG_KEYFRAME && (!has_first_keyframe_ || frame_count_ / (SECOND_PER_FILE * config.fps) > 0)) {
        frame_count_ = 0;
        has_first_keyframe_ = true;
        FinishFile();
        InitializeContainer();
        AsyncWriteFileBackground();
    }

    if (has_first_keyframe_) {
        frame_count_++;
        PushEncodedBuffer(buffer);
    }
}
