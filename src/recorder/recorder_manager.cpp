#include "recorder/recorder_manager.h"

#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"
#include "recorder/utils.h"

#include <mutex>

const int SECOND_PER_FILE = 30;

std::unique_ptr<RecorderManager> RecorderManager::Create(
        std::shared_ptr<V4L2Capture> video_src,
        std::shared_ptr<PaCapture> audio_src,
        std::string record_path) {
    auto p = std::make_unique<RecorderManager>(record_path);
    if (video_src) {
        p->CreateVideoRecorder(video_src, video_src->format());
        p->SubscribeVideoSource(video_src);
    }
    if (audio_src) {
        p->CreateAudioRecorder(audio_src);
    }
    return p;
}

void RecorderManager::CreateVideoRecorder(
    std::shared_ptr<V4L2Capture> capture, uint32_t format) {
    video_recorder = ([&, capture]() -> std::unique_ptr<VideoRecorder> {
        if (capture == nullptr) {
            return nullptr;
        } else if (format == V4L2_PIX_FMT_H264) {
            return RawH264Recorder::Create(capture);
        } else {
            return H264Recorder::Create(capture);
        }
    })();
}

void RecorderManager::CreateAudioRecorder(std::shared_ptr<PaCapture> capture) {
    audio_recorder = ([&, capture]() -> std::unique_ptr<AudioRecorder> {
        if (capture == nullptr) {
            return nullptr;
        }
        return AudioRecorder::CreateRecorder(capture);
    })();
}

RecorderManager::RecorderManager(std::string record_path)
    : frame_count(0),
      fmt_ctx(nullptr),
      record_path(record_path) {}

void RecorderManager::SubscribeVideoSource(std::shared_ptr<V4L2Capture> video_src) {
    observer = video_src->AsObservable();
    observer->Subscribe([&](Buffer buffer) {
        // if (fmt_ctx == nullptr || frame_count % (SECOND_PER_FILE * video_src->fps()) == 0) {
        //     printf("==============\n");
        //     Stop();
        //     Start();
        // }
        // printf("[%p]: %d\n", fmt_ctx, frame_count);
        // frame_count++;
    });
}

void RecorderManager::WriteIntoFile(AVPacket *pkt) {
    std::lock_guard<std::mutex> lock(mux);
    int ret;
    if ((ret = av_interleaved_write_frame(fmt_ctx, pkt)) < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        fprintf(stderr, "Error occurred: %s\n", err_buf);
    }
}

void RecorderManager::Start() {
    fmt_ctx = RecUtil::CreateContainer(record_path);
    if (video_recorder) {
        video_recorder->AddStream(fmt_ctx);
        video_recorder->OnEncoded([this](AVPacket *pkt) {
            this->WriteIntoFile(pkt);
        });
    }
    if (audio_recorder) {
        audio_recorder->AddStream(fmt_ctx);
        audio_recorder->OnEncoded([this](AVPacket *pkt) {
            this->WriteIntoFile(pkt);
        });
    }
    RecUtil::WriteFormatHeader(fmt_ctx);
    if (video_recorder) {
        video_recorder->Start();
    }
    if (audio_recorder) {
        audio_recorder->Start();
    }
}

void RecorderManager::Stop() {
    if (video_recorder) {
        video_recorder->Stop().get();
    }
    if (audio_recorder) {
        audio_recorder->Stop().get();
    }
    if (fmt_ctx) {
        RecUtil::CloseContext(fmt_ctx);
    }
    fmt_ctx = nullptr;
    frame_count = 0;
}

RecorderManager::~RecorderManager() {
    observer = nullptr;
    if (fmt_ctx) {
        RecUtil::CloseContext(fmt_ctx);
    }
}
