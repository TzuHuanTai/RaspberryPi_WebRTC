#ifndef BACKGROUND_RECODER_H_
#define BACKGROUND_RECODER_H_

#include "recorder/video_recorder.h"
#include "v4l2_codecs/v4l2_encoder.h"
#include "v4l2_codecs/v4l2_decoder.h"

#include <queue>

class BackgroundRecorder {
public:
    static std::unique_ptr<BackgroundRecorder> CreateBackgroundRecorder(
        std::shared_ptr<V4L2Capture> capture, RecorderFormat format);

    BackgroundRecorder(std::shared_ptr<V4L2Capture> capture, RecorderFormat format);
    ~BackgroundRecorder();
    void Start();
    void Stop();

private:
    int fps_;
    int width_;
    int height_;
    int frame_count_;
    bool abort_;
    RecorderFormat format_;
    std::queue<Buffer> buffer_queue_;
    std::unique_ptr<Worker> worker_;
    std::shared_ptr<V4L2Capture> capture_;
    std::unique_ptr<VideoRecorder> recorder_;
    std::unique_ptr<V4l2Decoder> decoder_;
    std::unique_ptr<V4l2Encoder> encoder_;

    void Init();
    void RecordingFunction();
    std::unique_ptr<VideoRecorder> CreateRecorder();
};

#endif
