#ifndef V4L2DMA_TRACK_SOURCE_H_
#define V4L2DMA_TRACK_SOURCE_H_

#include "track/v4l2_track_source.h"

#include <media/base/adapted_video_track_source.h>

class V4l2DmaTrackSource : public V4L2TrackSource {
public:
    static rtc::scoped_refptr<V4l2DmaTrackSource> Create(
        std::shared_ptr<V4L2Capture> capture);
    V4l2DmaTrackSource(std::shared_ptr<V4L2Capture> capture);
    void OnFrameCaptured(Buffer buffer) override;

protected:
    void Init() override;

private:
    std::unique_ptr<V4l2m2mScaler> scaler_;
    std::unique_ptr<V4l2m2mDecoder> decoder_;
};

#endif
