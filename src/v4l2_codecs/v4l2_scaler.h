#ifndef V4L2_SCALER_H_
#define V4L2_SCALER_H_

#include "v4l2_codecs/v4l2_codec.h"

// Linux
#include <linux/videodev2.h>
#include <stdint.h>

class V4l2Scaler : public V4l2Codec {
public:
    V4l2Scaler() {};
    ~V4l2Scaler() {};
    bool V4l2m2mConfigure(int src_width, int src_height, 
                          int dst_width, int dst_height,
                          bool is_drm_src, bool is_drm_dst);
};

#endif // V4L2_SCALER_H_
