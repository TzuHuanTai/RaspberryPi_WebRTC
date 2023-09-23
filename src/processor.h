#ifndef PROCESSOR_H_
#define PROCESSOR_H_

#include <rtc_base/platform_thread.h>

class Processor {
public:
    Processor(std::function<void()> excuting_function);
    ~Processor();
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
