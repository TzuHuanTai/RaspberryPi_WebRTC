#include "args.h"
#include "decoder/v4l2m2m_decoder.h"
#include "scaler/v4l2m2m_scaler.h"
#include "capture/v4l2_capture.h"

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <mutex>
#include <condition_variable>

void WriteImage(Buffer buffer, int index)
{
    printf("Dequeued buffer index: %d\n"
           "  bytesused: %d\n",
           index, buffer.length);

    std::string filename = "img" + std::to_string(index) + ".yuv";
    int outfd = open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if ((outfd == -1) && (EEXIST == errno))
    {
        /* open the existing file with write flag */
        outfd = open(filename.c_str(), O_WRONLY);
    }

    write(outfd, buffer.start, buffer.length);
}

int main(int argc, char *argv[])
{
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    bool wait_first_keyframe = false;
    int images_nb = 0;
    int record_sec = 1;
    Args args{.fps = 15,
              .width = 640,
              .height = 480,
              .v4l2_format = "h264"};
    auto capture = V4L2Capture::Create(args.device);

    V4l2m2mDecoder decoder;
    decoder.V4l2m2mConfigure(args.width, args.height, true);
    V4l2m2mScaler scaler;
    scaler.V4l2m2mConfigure(args.width, args.height, 320, 240, true, false);

    auto test = [&]() -> bool
    {
        std::unique_lock<std::mutex> lock(mtx);
        capture->CaptureImage();
        Buffer buffer = capture->GetImage();

        decoder.EmplaceBuffer(buffer, [&, images_nb](Buffer decoded_buffer) {
            scaler.EmplaceBuffer(decoded_buffer, [&, images_nb](Buffer scaled_buffer) {
                WriteImage(scaled_buffer, images_nb);
            });
        });

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
