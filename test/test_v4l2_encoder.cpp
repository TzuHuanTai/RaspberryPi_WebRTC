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
#include <chrono>

int main(int argc, char *argv[]) {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    bool has_first_keyframe_ = false;
    int images_nb = 0;
    int record_sec = 300;
    int dst_width = 960;
    int dst_heigh = 720;
    Args args{.fps = 30,
              .width = 1280,
              .height = 960,
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

    int cam_frame_count = 0;
    auto cam_start_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    observer->Subscribe([&](rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) {

        V4l2Buffer buffer((void*)frame_buffer->Data(), frame_buffer->size(),
                          frame_buffer->flags(), frame_buffer->timestamp());

        if (frame_buffer->format() == V4L2_PIX_FMT_H264 && !has_first_keyframe_) {
            if (buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
                has_first_keyframe_ = true;
            } else {
                return;
            }
        }

        auto cam_current_time = std::chrono::steady_clock::now();
        cam_frame_count++;

        if (std::chrono::duration_cast<std::chrono::seconds>(cam_current_time - cam_start_time).count() >= 1) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(cam_current_time - cam_start_time).count();
            double cam_current_fps = static_cast<double>(cam_frame_count) / (duration / 1000.0);
            cam_start_time = cam_current_time;
            printf("Camera FPS: %.2f\n", cam_current_fps);
            cam_frame_count = 0;
        }

        decoder->EmplaceBuffer(buffer, [&](V4l2Buffer &decoded_buffer) {
            scaler->EmplaceBuffer(decoded_buffer, [&](V4l2Buffer &scaled_buffer) {
                encoder->EmplaceBuffer(scaled_buffer, [&](V4l2Buffer &encoded_buffer) {
                    if (is_finished) {
                        return;
                    }

                    auto current_time = std::chrono::steady_clock::now();
                    frame_count++;

                    if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() >= 1) {
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
                        double current_fps = static_cast<double>(frame_count) / (duration / 1000.0);
                        start_time = current_time;
                        printf("Codec FPS: %.2f\n", current_fps);
                        frame_count = 0;
                    }

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
