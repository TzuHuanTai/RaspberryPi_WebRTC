#ifndef WORKER_H_
#define WORKER_H_

#include <rtc_base/platform_thread.h>

class Worker {
public:
    Worker(std::function<void()> excuting_function);
    ~Worker();
    void Release();
    void Run();

protected:
    std::function<void()> excuting_function_;

private:
    bool can_running_;
    rtc::PlatformThread thread_;

    void Thread();
};

#endif
