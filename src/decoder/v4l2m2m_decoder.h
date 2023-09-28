#ifndef V4L2M2M_DECODER_H_
#define V4L2M2M_DECODER_H_

#include "v4l2_utils.h"

// Linux
#include <linux/videodev2.h>
#include <stdint.h>

class V4l2m2mDecoder
{
public:
    explicit V4l2m2mDecoder();
    ~V4l2m2mDecoder();

    bool V4l2m2mConfigure(int width, int height);
    bool V4l2m2mDecode(const uint8_t *byte, uint32_t length, Buffer &buffer);
    void V4l2m2mRelease();

private:
    int fd_;
    int width_;
    int height_;
    int buffer_count_;
    BufferGroup output_;
    BufferGroup capture_;

    void HandleEvent();
};

#endif // V4L2M2M_DECODER_H_
