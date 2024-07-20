#ifndef SCALE_TRACK_SOURCE_H_
#define SCALE_TRACK_SOURCE_H_

#include "common/v4l2_utils.h"
#include "capture/v4l2_capture.h"

#include <media/base/adapted_video_track_source.h>
#include <rtc_base/timestamp_aligner.h>

class ScaleTrackSource : public rtc::AdaptedVideoTrackSource {
public:
    static rtc::scoped_refptr<ScaleTrackSource> Create(std::shared_ptr<V4L2Capture> capture);
    ScaleTrackSource(std::shared_ptr<V4L2Capture> capture);
    ~ScaleTrackSource();

    SourceState state() const override;
    bool remote() const override;
    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;
    void StartTrack();
    rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame();

protected:
    int width;
    int height;
    webrtc::VideoType src_video_type;
    std::shared_ptr<V4L2Capture> capture;
    rtc::TimestampAligner timestamp_aligner;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> i420_raw_buffer;

    virtual void OnFrameCaptured(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer);
};

#endif
