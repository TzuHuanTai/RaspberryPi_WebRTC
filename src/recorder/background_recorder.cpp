#include "recorder/background_recorder.h"
#include "recorder/h264_recorder.h"

const int SECOND_PER_FILE = 300;

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
      fps_(capture->fps()),
      width_(capture->width()),
      height_(capture->height()),
      frame_count_(0),
      abort_(false),
      format_(format) {}

void BackgroundRecorder::Init() {
    decoder_ = std::make_unique<V4l2Decoder>();
    decoder_->Configure(width_, height_, capture_->format(), true);

    encoder_ = std::make_unique<V4l2Encoder>();
    encoder_->Configure(width_, height_, true);

    recorder_ = CreateRecorder();

    auto observer = capture_->AsObservable();
    observer->Subscribe([&](Buffer buffer) {
        if (!abort_ || buffer_queue_.size() < 100) {
            buffer_queue_.push(buffer);
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

void BackgroundRecorder::RecordingFunction() {
    if(abort_ || buffer_queue_.empty()) {
        usleep(1000);
        return;
    }

    auto buffer = buffer_queue_.front();
    buffer_queue_.pop();

    if (frame_count_++ % (SECOND_PER_FILE * fps_) == 0) {
        recorder_ = CreateRecorder();
        V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
    }

    decoder_->EmplaceBuffer(buffer, [&](Buffer decoded_buffer) {
        encoder_->EmplaceBuffer(decoded_buffer, [&](Buffer encoded_buffer) {
            recorder_->PushEncodedBuffer(encoded_buffer);
        });
    });
}

void BackgroundRecorder::Start() {
    worker_.reset(new Worker([&]() { RecordingFunction();}));
    worker_->Run();
}

void BackgroundRecorder::Stop() {
    abort_ = true;
    decoder_.reset();
    encoder_.reset();
    recorder_.reset();
    worker_.reset();
}
