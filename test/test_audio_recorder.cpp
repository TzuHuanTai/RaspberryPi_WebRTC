#include "args.h"
#include "recorder/recorder_manager.h"

int main(int argc, char *argv[]) {
    Args args{.fps = 15,
              .width = 1280,
              .height = 720,
              .v4l2_format = "h264",
              .device = "/dev/video0",
              .record_path = "./"};
    
    auto video_capture = V4L2Capture::Create(args);
    auto audio_capture = PaCapture::Create();
    auto recorder_mgr = RecorderManager::Create(video_capture, audio_capture, 
                                                args.record_path);
    recorder_mgr->Start();
    sleep(150);
    recorder_mgr->Stop();

    return 0;
}
