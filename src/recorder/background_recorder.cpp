#include "recorder/background_recorder.h"
#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"

std::unique_ptr<BackgroundRecorder> BackgroundRecorder::CreateBackgroundRecorder(
    std::shared_ptr<V4L2Capture> capture, RecorderFormat format) {
    return std::make_unique<BackgroundRecorder>(capture, format);
}

BackgroundRecorder::BackgroundRecorder(
    std::shared_ptr<V4L2Capture> capture,
    RecorderFormat format) 
    : capture_(capture),
      format_(format) {}

BackgroundRecorder::~BackgroundRecorder() {
    Stop();
}

std::unique_ptr<VideoRecorder> BackgroundRecorder::CreateRecorder() {
    switch (format_) {
        case RecorderFormat::H264:
            if (capture_->format() == V4L2_PIX_FMT_H264) {
                return RawH264Recorder::Create(capture_);
            } else {
                return H264Recorder::Create(capture_);
            }
        case RecorderFormat::VP8:
        case RecorderFormat::AV1:
        default:
            return nullptr;
    }
}

void BackgroundRecorder::Start() {
    recorder_ = CreateRecorder();
    worker_.reset(new Worker([&]() { recorder_->RecordingLoop();}));
    worker_->Run();
}

void BackgroundRecorder::Stop() {
    recorder_.reset();
    worker_.reset();
}
