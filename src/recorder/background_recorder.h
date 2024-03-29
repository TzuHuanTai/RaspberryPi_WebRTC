#ifndef BACKGROUND_RECODER_H_
#define BACKGROUND_RECODER_H_

#include "recorder/video_recorder.h"

class BackgroundRecorder {
public:
    static std::unique_ptr<BackgroundRecorder> CreateBackgroundRecorder(
        std::shared_ptr<V4L2Capture> capture, RecorderFormat format);

    BackgroundRecorder(std::shared_ptr<V4L2Capture> capture, RecorderFormat format);
    ~BackgroundRecorder();
    void Start();
    void Stop();

private:
    RecorderFormat format_;
    std::unique_ptr<Worker> worker_;
    std::shared_ptr<V4L2Capture> capture_;
    std::unique_ptr<VideoRecorder> recorder_;

    bool CreateVideoFolder(const std::string& folder_path);
    void RotateFiles();
};

#endif
