#ifndef BACKGROUND_RECODER_H_
#define BACKGROUND_RECODER_H_

#include "conductor.h"
#include "recorder/recorder_manager.h"
#include <filesystem>

class BackgroundRecorder {
public:
    static std::unique_ptr<BackgroundRecorder> Create(
        std::shared_ptr<Conductor> conductor);

    BackgroundRecorder(std::shared_ptr<Conductor> capture);
    ~BackgroundRecorder();
    void Start();
    void Stop();

private:
    int max_files_;
    std::string record_path_;
    std::unique_ptr<Worker> worker_;
    std::shared_ptr<PaCapture> audio_capture_;
    std::shared_ptr<V4L2Capture> video_capture_;

    void RotateFiles(std::string folder_path, int max_files);
    void DeleteRedundantFiles(std::vector<std::filesystem::path> files, int max_files);
};

#endif
