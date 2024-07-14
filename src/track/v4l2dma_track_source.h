#ifndef V4L2DMA_TRACK_SOURCE_H_
#define V4L2DMA_TRACK_SOURCE_H_

#include "track/swscale_track_source.h"
#include "v4l2_codecs/v4l2_scaler.h"
#include "v4l2_codecs/v4l2_decoder.h"

#include <media/base/adapted_video_track_source.h>

class V4l2DmaTrackSource : public SwScaleTrackSource {
public:
    static rtc::scoped_refptr<V4l2DmaTrackSource> Create(
        std::shared_ptr<V4L2Capture> capture);
    V4l2DmaTrackSource(std::shared_ptr<V4L2Capture> capture);
    ~V4l2DmaTrackSource();

protected:
    void Init();
    void OnFrameCaptured(V4l2Buffer &buffer) override;

private:
    bool has_first_keyframe_;
    std::unique_ptr<V4l2Scaler> scaler_;
    std::unique_ptr<V4l2Decoder> decoder_;
};

#endif
