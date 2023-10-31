#ifndef RAW_BUFFER_ENCODER_H_
#define RAW_BUFFER_ENCODER_H_

#include "args.h"
#include "v4l2_codecs/v4l2_encoder.h"

#include <future>
#include <mutex>
// Linux
#include <linux/videodev2.h>

// WebRTC
#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include <rtc_base/synchronization/mutex.h>

class ProcessThread;

class RawBufferEncoder : public webrtc::VideoEncoder {
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
    int src_width_;
    int src_height_;
    int dst_width_;
    int dst_height_;
    std::mutex mtx_;
    std::unique_ptr<V4l2Encoder> encoder_;
    webrtc::VideoCodec codec_;
    webrtc::EncodedImage encoded_image_;
    webrtc::EncodedImageCallback *callback_;
    webrtc::BitrateAdjuster bitrate_adjuster_;

    void SendFrame(const webrtc::VideoFrame &frame, Buffer &encoded_buffer);
};

#endif // RAW_BUFFER_ENCODER_H_
