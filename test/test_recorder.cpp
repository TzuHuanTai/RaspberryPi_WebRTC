#include "v4l2_capture.h"
#include "recorder.h"
#include "args.h"

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
    bool isFinished = false;
    int images_nb = 0;
    int record_sec = 20;
    Args args{.fps = 15,
              .width = 1280,
              .height = 720,
              .use_i420_src = false};
    auto capture = V4L2Capture::Create(args.device);

    RecorderConfig config{.fps = args.fps,
                          .width = args.width,
                          .height = args.height,
                          .container = "mp4",
                          .encoder_name = "mjpeg"};
    Recorder recorder(config);

    auto test = [&]() -> bool
    {
        std::unique_lock<std::mutex> lock(mtx);
        Buffer buffer = capture->CaptureImage();
        recorder.PushBuffer(buffer);

        std::cout << '\r' << "Receive packet num: " << images_nb << "\e[K" << std::flush;

        if (images_nb++ < args.fps * record_sec)
        {
            return true;
        }
        else
        {
            isFinished = true;
            cond_var.notify_all();
            return false;
        }
    };

    (*capture).SetFormat(args.width, args.height, args.use_i420_src)
        .SetFps(args.fps)
        .SetCaptureFunc(test)
        .StartCapture();

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&]
                  { return isFinished; });

    return 0;
}
