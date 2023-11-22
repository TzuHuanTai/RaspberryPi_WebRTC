#ifndef RAW_BUFFER_ENCODER_H_
#define RAW_BUFFER_ENCODER_H_

#include "v4l2_codecs/v4l2m2m_encoder.h"

class RawBufferEncoder : public V4l2m2mEncoder {
public:
    RawBufferEncoder();
    ~RawBufferEncoder();

    int32_t InitEncode(const webrtc::VideoCodec *codec_settings,
                       const VideoEncoder::Settings &settings) override;
    int32_t Encode(const webrtc::VideoFrame &frame,
                   const std::vector<webrtc::VideoFrameType> *frame_types) override;
    webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;
};

#endif // RAW_BUFFER_ENCODER_H_
