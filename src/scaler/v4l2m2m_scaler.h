#ifndef V4L2M2M_SCALER_H_
#define V4L2M2M_SCALER_H_

#include "encoder/v4l2_codec.h"

// Linux
#include <linux/videodev2.h>
#include <stdint.h>

class V4l2m2mScaler : public V4l2Codec {
public:
    V4l2m2mScaler() {};
    ~V4l2m2mScaler() {};
    bool V4l2m2mConfigure(int src_width, int src_height, 
                          int dst_width, int dst_height,
                          bool is_drm_src, bool is_drm_dst);
};

#endif // V4L2M2M_SCALER_H_
