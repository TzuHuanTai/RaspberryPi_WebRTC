#include "track/h264_dma_track_source.h"
#include "v4l2_codecs/raw_buffer.h"

#include <future>

// WebRTC
#include <api/video/i420_buffer.h>
#include <api/video/video_frame_buffer.h>
#include <rtc_base/timestamp_aligner.h>
#include <third_party/libyuv/include/libyuv.h>

rtc::scoped_refptr<H264DmaTrackSource> H264DmaTrackSource::Create(
    std::shared_ptr<V4L2Capture> capture) {
    auto obj = rtc::make_ref_counted<H264DmaTrackSource>(std::move(capture));
    obj->Init();
    obj->StartTrack();
    return obj;
}

H264DmaTrackSource::H264DmaTrackSource(std::shared_ptr<V4L2Capture> capture)
    : V4l2DmaTrackSource(capture),
      has_first_keyframe_(false) {}

bool H264DmaTrackSource::HasFirstKeyFrame(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) {
    if (has_first_keyframe_) {
        return true;
    } else {
        has_first_keyframe_ = (frame_buffer->flags() & V4L2_BUF_FLAG_KEYFRAME) != 0;
    }
    return has_first_keyframe_;
}
