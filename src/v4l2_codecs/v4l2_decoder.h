#ifndef V4L2_DECODER_H_
#define V4L2_DECODER_H_

#include "v4l2_codecs/v4l2_codec.h"

// Linux
#include <linux/videodev2.h>
#include <stdint.h>

class V4l2Decoder : public V4l2Codec {
public:
    V4l2Decoder() {};
    ~V4l2Decoder() {};
    bool Configure(int width, int height, bool is_drm_dst);

protected:
    void HandleEvent() override;
};

#endif // V4L2_DECODER_H_