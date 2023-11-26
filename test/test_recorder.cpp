#include "capture/v4l2_capture.h"
#include "recorder/h264_recorder.h"
#include "recorder/background_recorder.h"
#include "args.h"

#include <condition_variable>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>

int main(int argc, char *argv[]) {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    int images_nb = 0;
    int record_sec = 10;
    Args args{.fps = 15,
              .width = 1280,
              .height = 720,
              .v4l2_format = "h264",
              .device = "/dev/video0",
              .record_path = "./"};
    
    auto start = std::chrono::steady_clock::now();
    auto elasped = std::chrono::steady_clock::now() - start;
    auto mili = std::chrono::duration_cast<std::chrono::milliseconds>(elasped);

    auto capture = V4L2Capture::Create(args);
    auto recorder = H264Recorder::Create(capture);

    auto observer = capture->AsObservable();
    observer->Subscribe([&](Buffer buffer) {
        if (images_nb++ < args.fps * record_sec) {
            recorder->PushEncodedBuffer(buffer);
            printf("Dequeue buffer number: %d\n"
                "  bytesused: %d in %ld ms\n",
                images_nb, buffer.length, mili.count());
        } else {
            is_finished = true;
            cond_var.notify_all();
        }
        elasped = std::chrono::steady_clock::now() - start;
        mili = std::chrono::duration_cast<std::chrono::milliseconds>(elasped);
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] { return is_finished; });
    recorder.reset();

    // ==== test background recorder ====
    auto bg_recorder = BackgroundRecorder::CreateBackgroundRecorder(capture, RecorderFormat::H264);
    bg_recorder->Start();
    sleep(record_sec);
    bg_recorder->Stop();

    capture.reset();
}
