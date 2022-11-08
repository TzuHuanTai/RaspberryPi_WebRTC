#include "../src/v4l2_capture.h"
#include "../src/recorder.h"
#include "../src/args.h"

#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

int main(int argc, char *argv[])
{
    std::mutex mtx;
    std::condition_variable cond_var;
    int images_nb = 0;
    int record_sec = 20;
    Args args{.fps = 15, .width = 1280, .height = 720};

    auto capture = V4L2Capture::Create(args.device);
    Recorder recorder(args);

    auto test = [&]() -> bool
    {
        std::unique_lock<std::mutex> lock(mtx);
        Buffer buffer = capture->CaptureImage();
        recorder.Write(buffer);

        std::cout << '\r' << "Receive packet num: " << images_nb << "\e[K" << std::flush;

        if(images_nb++ < args.fps * record_sec){
            return true;
        }else{
            cond_var.notify_all();
            return false;
        }
    };

    (*capture).SetFormat(args.width, args.height)
    .SetFps(args.fps)
    .SetCaptureFunc(test)
    .StartCapture();

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&]{return !capture->captureStarted;});

    return 0;
}
