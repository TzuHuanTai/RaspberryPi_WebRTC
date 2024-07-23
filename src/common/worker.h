#ifndef WORKER_H_
#define WORKER_H_

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <rtc_base/platform_thread.h>

class Worker {
public:
    Worker(std::string name, std::function<void()> excuting_function);
    ~Worker();
    void Release();
    void Run();

private:
    std::atomic<bool> abort_;
    std::string name_;
    std::function<void()> executing_function_;
    rtc::PlatformThread thread_;

    void Thread();
};

#endif
