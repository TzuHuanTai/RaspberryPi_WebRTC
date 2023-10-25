#ifndef V4L2M2M_ENCODER_H_
#define V4L2M2M_ENCODER_H_

#include "args.h"
#include "common/recorder.h"
#include "data_channel_subject.h"
#include "v4l2_codecs/v4l2_codec.h"

#include <functional>
#include <mutex>
#include <queue>

// WebRTC
#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include <rtc_base/platform_thread.h>

class V4l2m2mEncoder : public V4l2Codec, public webrtc::VideoEncoder {
public:
    V4l2m2mEncoder();
    ~V4l2m2mEncoder() override;

    int32_t InitEncode(const webrtc::VideoCodec *codec_settings,
                       const VideoEncoder::Settings &settings) override;
    int32_t RegisterEncodeCompleteCallback(
        webrtc::EncodedImageCallback *callback) override;
    int32_t Release() override;
    int32_t Encode(
        const webrtc::VideoFrame &frame,
        const std::vector<webrtc::VideoFrameType> *frame_types) override;
    void SetRates(const RateControlParameters &parameters) override;
    webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

    void RegisterRecordingObserver(std::shared_ptr<Observable<char *>> observer,
                                   Args args);

    bool V4l2m2mConfigure(int width, int height, bool is_drm_src);
    void V4l2m2mSetFps(const RateControlParameters &parameters);
    std::string name;

private:
    int width_;
    int height_;
    int framerate_;
    int bitrate_bps_;
    int key_frame_interval_;
    bool is_recording_;
    std::mutex mtx_;
    std::mutex recording_mtx_;
    std::unique_ptr<Recorder> recorder_;
    Args args_;

    void WriteFile(Buffer encoded_buffer);
    void EnableRecorder(bool onoff);
    void SendFrame(const webrtc::VideoFrame &frame, Buffer &encoded_buffer);

    webrtc::VideoCodec codec_;
    webrtc::EncodedImage encoded_image_;
    webrtc::EncodedImageCallback *callback_;
    webrtc::BitrateAdjuster bitrate_adjuster_;
    std::shared_ptr<Observable<char *>> observer_;
};

#endif // V4L2M2M_ENCODER_H_
