#ifndef BACKGROUND_RECODER_H_
#define BACKGROUND_RECODER_H_

#include "recorder/recorder_manager.h"

class BackgroundRecorder {
public:
    static std::unique_ptr<BackgroundRecorder> CreateBackgroundRecorder(
        std::shared_ptr<V4L2Capture> capture, RecorderFormat format);

    BackgroundRecorder(std::shared_ptr<V4L2Capture> capture, RecorderFormat format);
    ~BackgroundRecorder();
    void Start();
    void Stop();

private:
    Args args_;
    RecorderFormat format_;
    std::unique_ptr<Worker> worker_;
    std::shared_ptr<PaCapture> audio_capture_;
    std::shared_ptr<V4L2Capture> video_capture_;
    std::unique_ptr<RecorderManager> recorder_mgr_;

    bool CreateVideoFolder(const std::string& folder_path);
    void RotateFiles();
};

#endif
