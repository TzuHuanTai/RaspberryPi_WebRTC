#ifndef RAW_H264_RECODER_H_
#define RAW_H264_RECODER_H_

#include "recorder/video_recorder.h"

class RawH264Recorder : public VideoRecorder {
public:
    static std::unique_ptr<RawH264Recorder> Create(std::shared_ptr<V4L2Capture> capture);
    RawH264Recorder(std::shared_ptr<V4L2Capture> capture, std::string encoder_name);
    ~RawH264Recorder();

protected:
    void Encode(Buffer buffer) override;

private:
    int frame_count_;
    bool has_first_keyframe_;
};

#endif
