#include "recorder/background_recorder.h"
#include "recorder/h264_recorder.h"

std::unique_ptr<BackgroundRecorder> BackgroundRecorder::CreateBackgroundRecorder(
    std::shared_ptr<V4L2Capture> capture, RecorderFormat format) {
    auto ptr = std::make_unique<BackgroundRecorder>(capture, format);
    ptr->Init();
    return ptr;
}

BackgroundRecorder::BackgroundRecorder(
    std::shared_ptr<V4L2Capture> capture,
    RecorderFormat format) 
    : capture_(capture),
      format_(format) {}

void BackgroundRecorder::Init() {
    auto observer = capture_->AsObservable();
    observer->Subscribe([&](Buffer buffer) {
        // todo: copy to a queue
        if (recorder_ != nullptr) {
            recorder_->PushEncodedBuffer(buffer);
        }
    });
}

std::unique_ptr<VideoRecorder> BackgroundRecorder::CreateRecorder() {
    switch (format_) {
        case RecorderFormat::H264:
            return H264Recorder::Create(capture_);
        case RecorderFormat::VP8:
        case RecorderFormat::AV1:
        default:
            return nullptr;
    }
}

void BackgroundRecorder::Start() {
    recorder_ = CreateRecorder();
}

void BackgroundRecorder::Stop() {
    recorder_.reset();
}
