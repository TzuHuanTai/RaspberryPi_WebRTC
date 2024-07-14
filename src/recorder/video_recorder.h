#ifndef VIDEO_RECODER_H_
#define VIDEO_RECODER_H_

#include "args.h"
#include "common/v4l2_utils.h"
#include "recorder/recorder.h"
#include "v4l2_codecs/v4l2_decoder.h"

#include <atomic>
#include <queue>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class VideoRecorder : public Recorder<V4l2Buffer> {
public:
    VideoRecorder(Args config, std::string encoder_name);
    virtual ~VideoRecorder() {};
    void OnBuffer(V4l2Buffer &buffer) override;
    void PostStop() override;
    void SetFilename(std::string &name);

protected:
    Args config;
    std::atomic<int> feeded_frames;
    bool has_first_keyframe;
    std::string filename;
    std::string encoder_name;
    std::queue<V4l2Buffer> raw_buffer_queue;

    AVRational frame_rate;

    virtual void Encode(V4l2Buffer &buffer) = 0;

    void OnEncoded(V4l2Buffer &buffer);
    void SetBaseTimestamp(struct timeval time);

private:
    struct timeval base_time_;
    std::unique_ptr<V4l2Decoder> image_decoder_;

    void InitializeEncoderCtx(AVCodecContext* &encoder) override;
    bool ConsumeBuffer() override;
    void MakePreviewImage(V4l2Buffer &raw_buffer);
    void InitializeImageDecoder();
    void SavePreviewImage(V4l2Buffer &decoded_buffer);
};

#endif  // VIDEO_RECODER_H_
