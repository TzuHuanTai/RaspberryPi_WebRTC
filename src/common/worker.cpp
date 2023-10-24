#include "worker.h"

Worker::Worker(std::function<void()> excuting_function)
    : can_running_(true),
      excuting_function_(excuting_function) {}

Worker::~Worker() {
    Release();
}

void Worker::Release() {
    can_running_ = false;
    if (!thread_.empty()) {
        thread_.Finalize();
    }
}

void Worker::Run() {
    thread_ = rtc::PlatformThread::SpawnJoinable(
            [this]()
            { this->Thread(); },
            "CaptureThread",
            rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));
}

void Worker::Thread() {
    while (can_running_) {
        excuting_function_();
    }
}
