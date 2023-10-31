#include "args.h"
#include "v4l2_codecs/v4l2_decoder.h"
#include "v4l2_codecs/v4l2_scaler.h"
#include "capture/v4l2_capture.h"

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <mutex>
#include <condition_variable>

void WriteImage(Buffer buffer, int index) {
    printf("Dequeued buffer index: %d\n"
           "  bytesused: %d\n",
           index, buffer.length);

    std::string filename = "img" + std::to_string(index) + ".yuv";
    int outfd = open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if ((outfd == -1) && (EEXIST == errno)) {
        /* open the existing file with write flag */
        outfd = open(filename.c_str(), O_WRONLY);
    }

    write(outfd, buffer.start, buffer.length);
}

int main(int argc, char *argv[]) {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    int images_nb = 0;
    int record_sec = 1;
    Args args{.fps = 15,
              .width = 640,
              .height = 480,
              .v4l2_format = "h264"};

    auto decoder = std::make_unique<V4l2Decoder>();
    decoder->Configure(args.width, args.height, V4L2_PIX_FMT_H264, true);
    auto scaler = std::make_unique<V4l2Scaler>();
    scaler->Configure(args.width, args.height, 320, 240, true, false);

    auto capture = V4L2Capture::Create(args);
    auto observer = capture->AsObservable();
    observer->Subscribe([&](Buffer buffer) {
        decoder->EmplaceBuffer(buffer, [&](Buffer decoded_buffer) {
            scaler->EmplaceBuffer(decoded_buffer, [&](Buffer scaled_buffer) {
                if (is_finished) {
                    return;
                }

                if (images_nb++ < args.fps * record_sec) {
                    WriteImage(scaled_buffer, images_nb);
                } else {
                    is_finished = true;
                    cond_var.notify_all();
                }
            });
        });
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] { return is_finished; });

    return 0;
}
