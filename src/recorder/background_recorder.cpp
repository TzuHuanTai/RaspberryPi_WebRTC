#include "recorder/background_recorder.h"

#include <iostream>
#include <vector>
#include <algorithm>

std::unique_ptr<BackgroundRecorder> BackgroundRecorder::CreateBackgroundRecorder(
    std::shared_ptr<V4L2Capture> capture) {
    return std::make_unique<BackgroundRecorder>(capture);
}

BackgroundRecorder::BackgroundRecorder(std::shared_ptr<V4L2Capture> capture) 
    : args_(capture->config()),
      video_capture_(capture) {}

BackgroundRecorder::~BackgroundRecorder() {
    Stop();
}

bool BackgroundRecorder::CreateVideoFolder(const std::string& folder_path) {
    if (!std::filesystem::exists(folder_path)) {
        if (!std::filesystem::create_directory(folder_path)) {
            std::cerr << "Failed to create directory: " << folder_path << std::endl;
            return false;
        }
        std::cout << "Directory created: " << folder_path << std::endl;
    } else {
        std::cout << "Directory already exists: " << folder_path << std::endl;
    }
    return true;
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
    if (CreateVideoFolder(args_.record_path)) {
        audio_capture_ = PaCapture::Create(args_);
        RecorderManager::Create(video_capture_, audio_capture_, 
                                args_.record_path);
        worker_.reset(new Worker([this]() { 
            RotateFiles(args_.record_path, args_.max_files);
            sleep(60);
        }));
        worker_->Run();
    } else {
        std::cout << "Background recorder is not started!" << std::endl;
    }
}

void BackgroundRecorder::Stop() {
    worker_.reset();
}
