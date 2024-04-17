#include "recorder/recorder_manager.h"

#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"
#include "recorder/utils.h"

const int SECOND_PER_FILE = 300;

std::unique_ptr<RecorderManager> RecorderManager::Create(
        std::shared_ptr<V4L2Capture> video_src,
        std::shared_ptr<PaCapture> audio_src,
        std::string record_path) {
    auto p = std::make_unique<RecorderManager>(video_src, audio_src, record_path);
    if (video_src) {
        p->CreateVideoRecorder(video_src);
        p->SubscribeVideoSource(video_src);
    }
    if (audio_src) {
        p->CreateAudioRecorder(audio_src);
        p->SubscribeAudioSource(audio_src);
    }
    return p;
}

void RecorderManager::CreateVideoRecorder(
    std::shared_ptr<V4L2Capture> capture) {
    fps = capture->fps();
    video_recorder = ([&, capture]() -> std::unique_ptr<VideoRecorder> {
        if (capture->format() == V4L2_PIX_FMT_H264) {
            return RawH264Recorder::Create(capture->config());
        } else {
            return H264Recorder::Create(capture->config());
        }
    })();
}

void RecorderManager::CreateAudioRecorder(std::shared_ptr<PaCapture> capture) {
    audio_recorder = ([&]() -> std::unique_ptr<AudioRecorder> {
        return AudioRecorder::Create(capture->config());
    })();
}

RecorderManager::RecorderManager(std::shared_ptr<V4L2Capture> video_src, 
            std::shared_ptr<PaCapture> audio_src,
            std::string record_path)
    : frame_count(0),
      fmt_ctx(nullptr),
      has_first_keyframe(false),
      record_path(record_path),
      video_src(video_src),
      audio_src(audio_src) {}

void RecorderManager::SubscribeVideoSource(std::shared_ptr<V4L2Capture> video_src) {
    video_observer = video_src->AsObservable();
    video_observer->Subscribe([&](Buffer buffer) {
        // waiting first keyframe to start recorders.
        if (!has_first_keyframe && frame_count == 0 &&
            buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
            Start();
        }

        // write in new the new file.
        if (frame_count / (SECOND_PER_FILE * fps) > 0 && 
            buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
            Stop();
            Start();
        }

        if (has_first_keyframe && video_recorder) {
            video_recorder->OnBuffer(buffer);
            frame_count++;
        }
    });
}

void RecorderManager::SubscribeAudioSource(std::shared_ptr<PaCapture> audio_src) {
    audio_observer = audio_src->AsObservable();
    audio_observer->Subscribe([&](PaBuffer buffer) {
        if (has_first_keyframe && audio_recorder) {
            audio_recorder->OnBuffer(buffer);
        }
    });
}

void RecorderManager::WriteIntoFile(AVPacket *pkt) {
    std::lock_guard<std::mutex> lock(ctx_mux);
    int ret;
    if (fmt_ctx && fmt_ctx->nb_streams > pkt->stream_index &&
        (ret = av_interleaved_write_frame(fmt_ctx, pkt)) < 0) {
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

    has_first_keyframe = true;
}

void RecorderManager::Stop() {
    if (fmt_ctx) {
        RecUtil::CloseContext(fmt_ctx);
    }
    fmt_ctx = nullptr;
    frame_count = 0;
    if (video_recorder) {
        video_recorder->ResetCodecs();
    }
    if (audio_recorder) {
        audio_recorder->ResetCodecs();
    }
}

RecorderManager::~RecorderManager() {
    video_recorder.reset();
    audio_recorder.reset();
    video_observer->UnSubscribe();
    audio_observer->UnSubscribe();
    Stop();
}
