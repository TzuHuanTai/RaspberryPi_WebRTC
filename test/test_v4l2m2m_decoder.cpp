#include "args.h"
#include "decoder/v4l2m2m_decoder.h"
#include "capture/v4l2_capture.h"

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <mutex>
#include <condition_variable>

int main(int argc, char *argv[])
{
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    bool wait_first_keyframe = false;
    int images_nb = 0;
    int record_sec = 3;
    struct Buffer decoded_buffer;
    Args args{.fps = 30,
              .width = 640,
              .height = 480,
              .v4l2_format = "h264"};
    auto capture = V4L2Capture::Create(args.device);

    V4l2m2mDecoder decoder;
    decoder.V4l2m2mConfigure(args.width, args.height, args.fps);

    auto test = [&]() -> bool
    {
        std::unique_lock<std::mutex> lock(mtx);
        capture->CaptureImage();
        Buffer buffer = capture->GetImage();

        decoder.V4l2m2mDecode((uint8_t *)buffer.start, buffer.length, decoded_buffer);
        printf("V4l2Capture get %d buffer: %p with %d length\n",
               images_nb, &(decoded_buffer.start), decoded_buffer.length);

        if (images_nb++ < args.fps * record_sec)
        {
            return true;
        }
        else
        {
            is_finished = true;
            cond_var.notify_all();
            return false;
        }
    };

    (*capture).SetFormat(args.width, args.height, args.v4l2_format)
        .SetFps(args.fps)
        .SetCaptureFunc(test)
        .StartCapture();

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&]
                  { return is_finished; });

    return 0;
}
