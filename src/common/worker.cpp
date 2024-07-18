#include "common/worker.h"
#include "common/logging.h"

Worker::Worker(std::string name, std::function<void()> executing_function)
    : abort_(false),
      name_(name),
      executing_function_(executing_function) {}

Worker::~Worker() {
    Release();
}

void Worker::Release() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        abort_ = true;
    }
    cond_var_.notify_all();
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
    std::unique_lock<std::mutex> lock(mtx_);
    while (!abort_) {
        cond_var_.wait_for(lock, std::chrono::milliseconds(10), [this] { return abort_.load(); });
        if (!abort_) {
            executing_function_();
        }
    }
}
