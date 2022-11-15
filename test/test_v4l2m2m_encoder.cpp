#include "args.h"
#include "encoder/v4l2m2m_encoder.h"
#include "recorder.h"
#include "v4l2_capture.h"

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
    int record_sec = 10;
    Args args{.fps = 30,
              .width = 640,
              .height = 480,
              .use_i420_src = true,
              .use_h264_hw_encoder = true,
              .packet_type = "h264_v4l2m2m"};

    auto capture = V4L2Capture::Create(args.device);
    V4l2m2mEncoder encoder;
    encoder.V4l2m2mConfigure(args.width, args.height, args.fps);
    Recorder recorder(args);

    auto test = [&]() -> bool
    {
        std::unique_lock<std::mutex> lock(mtx);
        Buffer buffer = capture->CaptureImage();

        Buffer encoded_buffer =
            encoder.V4l2m2mEncode((uint8_t *)buffer.start, buffer.length);
        printf("V4l2Capture get %d buffer: %p with %d length\n",
               images_nb, &(encoded_buffer.start), encoded_buffer.length);

        recorder.Write(encoded_buffer);

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
