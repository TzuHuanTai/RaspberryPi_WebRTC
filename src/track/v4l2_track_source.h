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
private:
    int frame_nums_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::duration elasped_time_;
    std::chrono::milliseconds elasped_milli_time_;
    bool capture_started = false;
    webrtc::Mutex capture_lock_;

protected:
    std::shared_ptr<V4L2Capture> capture_;
    rtc::PlatformThread capture_thread_;
    std::function<bool()> capture_func_;

    void TrackThread();
    bool TrackProcess();

public:
    int fps_;
    int width_;
    int height_;
    webrtc::VideoType capture_video_type_;

    V4L2TrackSource(std::shared_ptr<V4L2Capture> capture);
    ~V4L2TrackSource();

    SourceState state() const override;
    bool remote() const override;
    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;
    virtual void OnFrameCaptured();

    static rtc::scoped_refptr<V4L2TrackSource> Create(std::shared_ptr<V4L2Capture> capture);
    void StartTrack();
};

#endif
