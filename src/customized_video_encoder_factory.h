#ifndef CUSTOMIZED_VIDEO_ENCODER_FACTORY_H_
#define CUSTOMIZED_VIDEO_ENCODER_FACTORY_H_

#include <api/video_codecs/sdp_video_format.h>
#include <api/video_codecs/video_encoder.h>
#include <api/video_codecs/video_encoder_factory.h>

#include <memory>
#include <vector>

std::unique_ptr<webrtc::VideoEncoderFactory> CreateCustomizedVideoEncoderFactory();

class CustomizedVideoEncoderFactory : public webrtc::VideoEncoderFactory
{
public:
    CustomizedVideoEncoderFactory(){};
    ~CustomizedVideoEncoderFactory(){};

    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
        const webrtc::SdpVideoFormat &format) override;
};

#endif // CUSTOMIZED_VIDEO_ENCODER_FACTORY_H_
