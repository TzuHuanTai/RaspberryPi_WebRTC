#include "args.h"
#include "capture/v4l2_capture.h"
#include "v4l2_codecs/v4l2_decoder.h"
#include "v4l2_codecs/v4l2_scaler.h"
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
    bool has_first_keyframe_ = false;
    int images_nb = 0;
    int record_sec = 300;
    int dst_width = 1440;
    int dst_heigh = 816;
    Args args{.fps = 30,
              .width = 1920,
              .height = 1088,
              .v4l2_format = "h264",
              .device = "/dev/video0"};

    auto decoder = std::make_unique<V4l2Decoder>();
    auto scaler = std::make_unique<V4l2Scaler>();
    auto encoder = std::make_unique<V4l2Encoder>();
    auto capture = V4L2Capture::Create(args);
    auto observer = capture->AsObservable();

    decoder->Configure(args.width, args.height, V4L2_PIX_FMT_H264, true);
    scaler->Configure(args.width, args.height, dst_width, dst_heigh, true, true);
    encoder->Configure(dst_width, dst_heigh, true);
    decoder->Start();
    scaler->Start();
    encoder->Start();

    observer->Subscribe([&](rtc::scoped_refptr<V4l2FrameBuffer> &frame_buffer) {

        V4l2Buffer buffer((void*)frame_buffer->Data(), frame_buffer->size(),
                          frame_buffer->flags(), frame_buffer->timestamp());

        if (frame_buffer->format() == V4L2_PIX_FMT_H264 && !has_first_keyframe_) {
            if (buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
                has_first_keyframe_ = true;
            } else {
                return;
            }
        }

        decoder->EmplaceBuffer(buffer, [&](V4l2Buffer &decoded_buffer) {
            scaler->EmplaceBuffer(decoded_buffer, [&](V4l2Buffer &scaled_buffer) {
                encoder->EmplaceBuffer(scaled_buffer, [&](V4l2Buffer &encoded_buffer) {
                    if (is_finished) {
                        return;
                    }

                    printf("\rencoder get %d buffer: %p with %d length",
                    images_nb, &(encoded_buffer.start), encoded_buffer.length);

                    if (images_nb++ > args.fps * record_sec) {
                        is_finished = true;
                        cond_var.notify_all();
                    }
                });
            });
        });
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] { return is_finished; });

    printf("\n");

    return 0;
}
