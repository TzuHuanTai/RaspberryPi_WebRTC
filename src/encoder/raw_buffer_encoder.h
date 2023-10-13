#ifndef RAW_BUFFER_ENCODER_H_
#define RAW_BUFFER_ENCODER_H_

#include "args.h"
#include "scaler/v4l2m2m_scaler.h"
#include "decoder/v4l2m2m_decoder.h"
#include "encoder/v4l2m2m_encoder.h"

#include <chrono>
#include <memory>

// Linux
#include <linux/videodev2.h>

// WebRTC
#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include <rtc_base/synchronization/mutex.h>

class ProcessThread;

class RawBufferEncoder : public webrtc::VideoEncoder
{
public:
    explicit RawBufferEncoder(const webrtc::SdpVideoFormat &format, Args args);
    ~RawBufferEncoder() override;

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

private:
    Args args_;
    int src_width_;
    int src_height_;
    int dst_width_;
    int dst_height_;
    std::unique_ptr<V4l2m2mScaler> scaler_;
    std::unique_ptr<V4l2m2mEncoder> encoder_;
    webrtc::VideoCodec codec_;
    webrtc::EncodedImage encoded_image_;
    webrtc::EncodedImageCallback *callback_;

    void SendFrame(const webrtc::VideoFrame &frame, Buffer &encoded_buffer);
};

#endif // RAW_BUFFER_ENCODER_H_
