#ifndef V4L2_TRACK_SOURCE_H_
#define V4L2_TRACK_SOURCE_H_

#include "v4l2_utils.h"
#include "capture/v4l2_capture.h"

#include <media/base/adapted_video_track_source.h>

class V4L2TrackSource : public rtc::AdaptedVideoTrackSource
{
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
    void OnFrameCaptured(Buffer buffer);

    static rtc::scoped_refptr<V4L2TrackSource> Create(std::shared_ptr<V4L2Capture> capture);
    void StartTrack();

private:
    std::shared_ptr<V4L2Capture> capture_;
};

#endif
