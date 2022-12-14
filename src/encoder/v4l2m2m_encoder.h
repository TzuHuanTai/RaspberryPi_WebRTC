#ifndef V4L2M2M_ENCODER_H_
#define V4L2M2M_ENCODER_H_

#include "v4l2_utils.h"
#include "data_channel_subject.h"

// Linux
#include <linux/videodev2.h>

// WebRTC
#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include "rtc_base/synchronization/mutex.h"

class V4l2m2mEncoder : public webrtc::VideoEncoder
{
public:
    explicit V4l2m2mEncoder(std::shared_ptr<Observable> observer = nullptr);
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

    void OnMessage(char *message);
    int32_t V4l2m2mConfigure(int width, int height, int fps);
    Buffer V4l2m2mEncode(const uint8_t *byte, uint32_t length);
    void V4l2m2mRelease();

private:
    int fd_;
    int32_t width_;
    int32_t height_;
    webrtc::VideoCodec codec_;
    uint32_t framerate_;
    uint32_t bitrate_bps_;
    int key_frame_interval_;
    Buffer output_;
    Buffer capture_;

    webrtc::EncodedImage encoded_image_;
    webrtc::EncodedImageCallback *callback_;
    std::unique_ptr<webrtc::BitrateAdjuster> bitrate_adjuster_;
    std::shared_ptr<Observable> observer_;
};

#endif // V4L2M2M_ENCODER_H_
