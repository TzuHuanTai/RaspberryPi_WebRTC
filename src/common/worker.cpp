#include "worker.h"

Worker::Worker(std::function<void()> excuting_function)
    : abort_(false),
      excuting_function_(excuting_function) {}

Worker::~Worker() {
    Release();
}

void Worker::Release() {
    abort_ = true;
    thread_.Finalize();
}

void Worker::Run() {
    thread_ = rtc::PlatformThread::SpawnJoinable(
            [this]()
            { this->Thread(); },
            "CaptureThread",
            rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));
}

void Worker::Thread() {
    while (!abort_) {
        excuting_function_();
    }
}
