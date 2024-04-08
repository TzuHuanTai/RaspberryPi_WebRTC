#include "args.h"
#include "recorder/recorder_manager.h"

int main(int argc, char *argv[]) {
    Args args{.fps = 15,
              .width = 1280,
              .height = 720,
              .sample_rate = 48000,
              .v4l2_format = "h264",
              .device = "/dev/video0",
              .record_path = "./"};
    
    auto video_capture = V4L2Capture::Create(args);
    auto audio_capture = PaCapture::Create(args);
    auto recorder_mgr = RecorderManager::Create(video_capture, audio_capture, 
                                                args.record_path);
    sleep(150);

    return 0;
}
