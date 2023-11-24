#ifndef H264_RECODER_H_
#define H264_RECODER_H_

#include "recorder/video_recorder.h"

class H264Recorder : public VideoRecorder {
public:
    static std::unique_ptr<H264Recorder> Create(std::shared_ptr<V4L2Capture> capture);

    H264Recorder(std::shared_ptr<V4L2Capture> capture, std::string encoder_name)
        : VideoRecorder(capture, encoder_name) {};
    ~H264Recorder() {};
};

#endif
