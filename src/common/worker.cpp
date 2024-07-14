#include "worker.h"
#include "common/logging.h"

Worker::Worker(std::string name, std::function<void()> excuting_function)
    : abort_(false),
      name_(name),
      excuting_function_(excuting_function) {}

Worker::~Worker() {
    Release();
}

void Worker::Release() {
    abort_ = true;
    thread_.Finalize();
    DEBUG_PRINT("'%s' was released!", name_.c_str());
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
