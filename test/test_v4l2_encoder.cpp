#include "args.h"
#include "common/recorder.h"
#include "capture/v4l2_capture.h"
#include "v4l2_codecs/v4l2_encoder.h"

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <mutex>
#include <condition_variable>

int main(int argc, char *argv[]) {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    bool wait_first_keyframe = false;
    int images_nb = 0;
    int record_sec = 10;
    struct Buffer encoded_buffer;
    Args args{.fps = 30,
              .width = 640,
              .height = 480,
              .v4l2_format = "i420",
              .record_path = "./",
              .record_container = "mp4",
              .encoder_name = "h264_v4l2m2m"};

    auto encoder = std::make_unique<V4l2Encoder>();
    encoder->Configure(args.width, args.height, false);

    auto recorder = std::make_unique<Recorder>(args);

    auto capture = V4L2Capture::Create(args);
    auto observer = capture->AsObservable();
    observer->Subscribe([&](Buffer buffer) {
        encoder->EmplaceBuffer(buffer, [&](Buffer encoded_buffer) {
            if (is_finished) {
                return;
            }

            printf("V4l2Capture get %d buffer: %p with %d length\n",
               images_nb, &(encoded_buffer.start), encoded_buffer.length);

            if (encoded_buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
                wait_first_keyframe = true;
            }

            if (wait_first_keyframe) {
                recorder->PushEncodedBuffer(encoded_buffer);
            }

            if (images_nb++ > args.fps * record_sec) {
                is_finished = true;
                cond_var.notify_all();
            }
        });
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] { return is_finished; });

    return 0;
}
