#include "recorder/background_recorder.h"

#include <iostream>
#include <vector>
#include <algorithm>

std::unique_ptr<BackgroundRecorder> BackgroundRecorder::Create(
    std::shared_ptr<Conductor> conductor) {
    return std::make_unique<BackgroundRecorder>(conductor);
}

BackgroundRecorder::BackgroundRecorder(std::shared_ptr<Conductor> conductor) 
    : max_files_(conductor->config().max_files),
      record_path_(conductor->config().record_path),
      audio_capture_(conductor->AudioSource()),
      video_capture_(conductor->VideoSource()) {}

BackgroundRecorder::~BackgroundRecorder() {
    Stop();
}

void BackgroundRecorder::RotateFiles(std::string folder_path, int max_files) {
    std::vector<std::filesystem::path> video_files;
    std::vector<std::filesystem::path> image_files;

    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        if (entry.path().extension() == ".mp4") {
            video_files.push_back(entry.path());
        } else if (entry.path().extension() == ".jpg") {
            image_files.push_back(entry.path());
        }
    }

    DeleteRedundantFiles(video_files, (max_files / 2) + 1);
    DeleteRedundantFiles(image_files, max_files / 2);
}

void BackgroundRecorder::DeleteRedundantFiles(std::vector<std::filesystem::path> files, int max_files) {
    if (files.size() > max_files) {
        std::sort(files.begin(), files.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b) {
            return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
        });

        int files_to_delete = files.size() - max_files;

        for (int i = 0; i < files_to_delete; ++i) {
            std::filesystem::remove(files[i]);
            std::cout << "Deleted file: " << files[i] << std::endl;
        }
    }
}

void BackgroundRecorder::Start() {
    RecorderManager::Create(video_capture_, audio_capture_, record_path_);
    worker_.reset(new Worker("BackgroundRecorder", [this]() {
        RotateFiles(record_path_, max_files_);
        sleep(60);
    }));
    worker_->Run();
}

void BackgroundRecorder::Stop() {
    std::cout << "[BackgroundRecorder] released." << std::endl;
    worker_.reset();
}
