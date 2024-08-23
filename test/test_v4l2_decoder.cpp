#include "args.h"
#include "v4l2_codecs/v4l2_decoder.h"
#include "v4l2_codecs/v4l2_scaler.h"
#include "capture/v4l2_capture.h"
#include "common/utils.h"

#include <third_party/libyuv/include/libyuv.h>
#include <jpeglib.h>

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <mutex>
#include <condition_variable>

void WriteJpegImage(Buffer buffer, int index) {
    std::string filename = "img" + std::to_string(index) + ".jpg";
    FILE* file = fopen(filename.c_str(), "wb");
    if (file) {
        fwrite((uint8_t*)buffer.start, 1, buffer.length, file);
        fclose(file);
        printf("JPEG data successfully written to %s\n", filename.c_str());
    } else {
        fprintf(stderr, "Failed to open file for writing: %s\n", filename.c_str());
    }
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
              .v4l2_format = "h264",
              .device = "/dev/video0"};

    int scaled_width = 640;
    int scaled_height = 480;

    auto decoder = std::make_unique<V4l2Decoder>();
    decoder->Configure(args.width, args.height, V4L2_PIX_FMT_H264, true);
    // auto scaler = std::make_unique<V4l2Scaler>();
    // scaler->Configure(args.width, args.height, scaled_width, scaled_height, true, false);

    auto capture = V4L2Capture::Create(args);
    auto observer = capture->AsRawBufferObservable();
    observer->Subscribe([&](V4l2Buffer buffer) {
        decoder->EmplaceBuffer(buffer, [&](V4l2Buffer decoded_buffer) {
            // scaler->EmplaceBuffer(decoded_buffer, [&](V4l2Buffer scaled_buffer) {
                if (is_finished) {
                    return;
                }

                if (images_nb++ < args.fps * record_sec) {
                    auto jpg_buffer = Utils::ConvertYuvToJpeg((uint8_t *)decoded_buffer.start, scaled_width, scaled_height);
                    printf("Buffer index: %d\n  bytesused: %d\n",images_nb, jpg_buffer.length);
                    WriteJpegImage(jpg_buffer, images_nb);
                } else {
                    is_finished = true;
                    cond_var.notify_all();
                }
            // });
        });
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] { return is_finished; });

    return 0;
}
