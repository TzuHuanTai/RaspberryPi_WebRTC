#ifndef H264_DMA_TRACK_SOURCE_H_
#define H264_DMA_TRACK_SOURCE_H_

#include <media/base/adapted_video_track_source.h>

#include "track/v4l2dma_track_source.h"
#include "v4l2_codecs/v4l2_decoder.h"
#include "v4l2_codecs/v4l2_scaler.h"

class H264DmaTrackSource : public V4l2DmaTrackSource {
  public:
    static rtc::scoped_refptr<H264DmaTrackSource> Create(std::shared_ptr<V4L2Capture> capture);
    H264DmaTrackSource(std::shared_ptr<V4L2Capture> capture);

  protected:
    bool HasFirstKeyFrame(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) override;

  private:
    bool has_first_keyframe_;
};

#endif
