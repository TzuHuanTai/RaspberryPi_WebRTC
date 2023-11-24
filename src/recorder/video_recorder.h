#ifndef VIDEO_RECODER_H_
#define VIDEO_RECODER_H_

#include "args.h"
#include "capture/v4l2_capture.h"
#include "common/v4l2_utils.h"

#include <string>
#include <future>
#include <queue>
#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

enum class RecorderFormat {
    H264,
    VP8,
    AV1,
    UNKNOWN
};

class VideoRecorder {
public:
    VideoRecorder(std::shared_ptr<V4L2Capture> capture, std::string encoder_name);
    ~VideoRecorder();

    void PushEncodedBuffer(Buffer buffer);

protected:
    Args config;
    int video_frame_count;
    bool is_recording;
    bool wait_first_keyframe;
    std::string encoder_name;
    std::future<void> consumer;
    std::queue<Buffer> buffer_queue;

    AVCodecContext *video_encoder;
    AVFormatContext *fmt_ctx;
    AVRational frame_rate;
    AVStream *video_st;

    std::string PrefixZero(int stc, int digits);
    std::string GenerateFilename();

    bool Initialize();
    void AddVideoStream();
    void ConsumeBuffer();
    void AsyncWriteFileBackground();
    bool Write(Buffer buffer);
    void Finish();
};

#endif // RECODER_H_
