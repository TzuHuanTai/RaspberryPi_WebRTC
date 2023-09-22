#include "processor.h"

Processor::Processor(std::function<void()> excuting_function)
    : can_running_(true),
      excuting_function_(excuting_function) {}

Processor::~Processor()
{
    Release();
}

void Processor::Release()
{
    can_running_ = false;
    if (!thread_.empty())
    {
        thread_.Finalize();
    }
}

void Processor::Run()
{
    thread_ = rtc::PlatformThread::SpawnJoinable(
            [this]()
            { this->Thread(); },
            "CaptureThread",
            rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));
}

void Processor::Thread()
{
    while (can_running_) {
        excuting_function_();
    }
}