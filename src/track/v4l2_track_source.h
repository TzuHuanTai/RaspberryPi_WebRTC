#ifndef V4L2_TRACK_SOURCE_H_
#define V4L2_TRACK_SOURCE_H_

#include "v4l2_utils.h"
#include "capture/v4l2_capture.h"

#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_defines.h>
#include <modules/video_capture/video_capture_impl.h>
#include <rtc_base/platform_thread.h>
#include <rtc_base/synchronization/mutex.h>
#include <media/base/adapted_video_track_source.h>
#include <media/base/video_adapter.h>

#include <chrono>

class V4L2TrackSource : public rtc::AdaptedVideoTrackSource
{
protected:
    std::shared_ptr<V4L2Capture> capture_;

public:
    int width_;
    int height_;
    webrtc::VideoType capture_video_type_;

    V4L2TrackSource(std::shared_ptr<V4L2Capture> capture);
    ~V4L2TrackSource();

    SourceState state() const override;
    bool remote() const override;
    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;
    virtual void OnFrameCaptured(Buffer buffer);

    static rtc::scoped_refptr<V4L2TrackSource> Create(std::shared_ptr<V4L2Capture> capture);
    void StartTrack();
};

#endif
