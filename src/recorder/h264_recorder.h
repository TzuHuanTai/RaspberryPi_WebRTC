#ifndef H264_RECODER_H_
#define H264_RECODER_H_

#include "recorder/video_recorder.h"
#include "v4l2_codecs/v4l2_decoder.h"
#include "v4l2_codecs/v4l2_encoder.h"

class H264Recorder : public VideoRecorder {
public:
    static std::unique_ptr<H264Recorder> Create(Args config);
    H264Recorder(Args config, std::string encoder_name);
    ~H264Recorder();
    void PreStart() override;

protected:
    void Encode(V4l2Buffer &buffer) override;

private:
    bool is_ready_;
    std::unique_ptr<V4l2Decoder> decoder_;
    std::unique_ptr<V4l2Encoder> encoder_;

    void ResetCodecs();
};

#endif
