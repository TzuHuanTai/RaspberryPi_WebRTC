#include "capture/v4l2_capture.h"
#include "recorder.h"
#include "args.h"

#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    int images_nb = 0;
    int record_sec = 5;
    Args args{.fps = 15,
              .width = 1280,
              .height = 720,
              .device = "/dev/video0"};

    auto capture = V4L2Capture::Create(args.device);

    RecorderConfig config{.fps = args.fps,
                          .width = args.width,
                          .height = args.height,
                          .container = "mp4",
                          .encoder_name = "mjpeg"};
    Recorder recorder(config);

    (*capture).SetFormat(args.width, args.height, "mjpeg")
        .SetFps(args.fps)
        .SetRotation(0)
        .StartCapture();
    
    auto start = std::chrono::steady_clock::now();
    auto elasped = std::chrono::steady_clock::now() - start;
    auto mili = std::chrono::duration_cast<std::chrono::milliseconds>(elasped);
    while (record_sec * 1000 >= mili.count())
    {
        if (images_nb * 1000 / args.fps < mili.count())
        {
            recorder.PushBuffer(capture->GetImage());
            printf("Dequeue buffer number: %d\n"
                "  bytesused: %d in %d ms\n",
                images_nb, capture->GetImage().length, mili);
    
            images_nb++;
        }
        usleep(args.fps);

        elasped = std::chrono::steady_clock::now() - start;
        mili = std::chrono::duration_cast<std::chrono::milliseconds>(elasped);
    }

    return 0;
}
