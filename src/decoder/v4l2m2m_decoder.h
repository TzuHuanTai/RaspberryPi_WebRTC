#ifndef V4L2M2M_DECODER_H_
#define V4L2M2M_DECODER_H_

#include "v4l2_utils.h"
#include "encoder/v4l2_codec.h"

// Linux
#include <linux/videodev2.h>
#include <stdint.h>

class V4l2m2mDecoder : public V4l2Codec
{
public:
    V4l2m2mDecoder() {};
    ~V4l2m2mDecoder() {};
    bool V4l2m2mConfigure(int width, int height);

protected:
    void HandleEvent() override;
};

#endif // V4L2M2M_DECODER_H_
