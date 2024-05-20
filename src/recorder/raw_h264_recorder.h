#ifndef RAW_H264_RECODER_H_
#define RAW_H264_RECODER_H_

#include "recorder/video_recorder.h"

class RawH264Recorder : public VideoRecorder {
public:
    static std::unique_ptr<RawH264Recorder> Create(Args config);
    RawH264Recorder(Args config, std::string encoder_name);
    ~RawH264Recorder();
    void ResetCodecs() override;
    bool CheckNALUnits(const Buffer& buffer);

protected:
    void Encode(Buffer buffer) override;

private:
    bool has_sps_;
    bool has_pps_;
    bool has_first_keyframe_;
};

#endif
