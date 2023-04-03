#ifndef H264CAPTURER_H_
#define H264CAPTURER_H_

#include "v4l2_capture.h"

class H264Capture : public V4L2Capture
{
public:
    H264Capture(std::string device);
    ~H264Capture() {};
    static rtc::scoped_refptr<V4L2Capture> Create(std::string device);

private:
    void OnFrameCaptured(Buffer buffer) override;
    H264Capture &SetFormat(uint width, uint height, bool use_i420_src = false) override;
};

#endif
