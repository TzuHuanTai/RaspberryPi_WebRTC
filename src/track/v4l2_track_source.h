#ifndef V4L2_TRACK_SOURCE_H_
#define V4L2_TRACK_SOURCE_H_

#include "common/v4l2_utils.h"
#include "capture/v4l2_capture.h"
#include "v4l2_codecs/v4l2m2m_scaler.h"
#include "v4l2_codecs/v4l2m2m_decoder.h"

#include <media/base/adapted_video_track_source.h>

class V4L2TrackSource : public rtc::AdaptedVideoTrackSource {
public:
    int width_;
    int height_;
    int config_width_;
    int config_height_;
    webrtc::VideoType src_video_type_;

    static rtc::scoped_refptr<V4L2TrackSource> Create(std::shared_ptr<V4L2Capture> capture);
    V4L2TrackSource(std::shared_ptr<V4L2Capture> capture);
    ~V4L2TrackSource();

    SourceState state() const override;
    bool remote() const override;
    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;
    void StartTrack();

protected:
    std::shared_ptr<V4L2Capture> capture_;
    virtual void Init() {};
    virtual void OnFrameCaptured(Buffer buffer);
};

#endif
