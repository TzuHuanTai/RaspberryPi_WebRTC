#ifndef V4L2M2M_DECODER_H_
#define V4L2M2M_DECODER_H_

#include "v4l2_utils.h"

// Linux
#include <linux/videodev2.h>
#include <stdint.h>

class V4l2m2mDecoder
{
public:
    explicit V4l2m2mDecoder(int32_t width, int32_t height);
    ~V4l2m2mDecoder();

    int32_t V4l2m2mConfigure();
    Buffer V4l2m2mDecode(const uint8_t *byte, uint32_t length);
    void V4l2m2mRelease();

private:
    int fd_;
    int32_t width_;
    int32_t height_;
    Buffer output_;
    Buffer capture_;
};

#endif // V4L2M2M_DECODER_H_
