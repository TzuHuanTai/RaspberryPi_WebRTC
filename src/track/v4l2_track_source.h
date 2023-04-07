#ifndef V4L2_TRACK_SOURCE_H_
#define V4L2_TRACK_SOURCE_H_

#include "v4l2_utils.h"

#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_defines.h>
#include <modules/video_capture/video_capture_impl.h>
#include <rtc_base/platform_thread.h>
#include <rtc_base/synchronization/mutex.h>
#include <media/base/adapted_video_track_source.h>
#include <media/base/video_adapter.h>

class V4L2TrackSource : public rtc::AdaptedVideoTrackSource
{
private:
    bool capture_started = false;
    webrtc::Mutex capture_lock_;

protected:
    Buffer *shared_buffers_;
    rtc::PlatformThread capture_thread_;
    std::function<bool()> capture_func_;

    void TrackThread();
    bool TrackProcess();

public:
    int fps_;
    int width_;
    int height_;
    webrtc::VideoType capture_video_type_;

    V4L2TrackSource(std::string viedo_type, Buffer *shared_buffers);
    ~V4L2TrackSource();

    SourceState state() const override;
    bool remote() const override;
    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;
    virtual void OnFrameCaptured(Buffer buffer);

    static rtc::scoped_refptr<V4L2TrackSource> Create(
        std::string device, 
        Buffer *shared_buffers);
    void StartTrack();
};

#endif
