#include "recorder/background_recorder.h"

#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

std::unique_ptr<BackgroundRecorder> BackgroundRecorder::CreateBackgroundRecorder(
    std::shared_ptr<V4L2Capture> capture, RecorderFormat format) {
    return std::make_unique<BackgroundRecorder>(capture, format);
}

BackgroundRecorder::BackgroundRecorder(
    std::shared_ptr<V4L2Capture> capture,
    RecorderFormat format) 
    : args_(capture->config()),
      video_capture_(capture),
      format_(format) {}

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

void BackgroundRecorder::RotateFiles() {
    auto folder_path = args_.record_path;
    const int max_files = args_.max_files;

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        if (entry.path().extension() == ".mp4" || entry.path().extension() == ".jpg") {
            files.push_back(entry.path());
        }
    }

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
    sleep(60);
}

void BackgroundRecorder::Start() {
    if (CreateVideoFolder(args_.record_path)) {
        audio_capture_ = PaCapture::Create(args_);
        recorder_mgr_ = RecorderManager::Create(video_capture_, audio_capture_, 
                                                args_.record_path);
        worker_.reset(new Worker([this]() { RotateFiles();}));
        worker_->Run();
    } else {
        std::cout << "Background recorder is not started!" << std::endl;
    }
}

void BackgroundRecorder::Stop() {
    recorder_mgr_.reset();
    worker_.reset();
}
