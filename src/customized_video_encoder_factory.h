#ifndef CUSTOMIZED_VIDEO_ENCODER_FACTORY_H_
#define CUSTOMIZED_VIDEO_ENCODER_FACTORY_H_

#include "args.h"

#include <api/video_codecs/video_encoder_factory.h>

std::unique_ptr<webrtc::VideoEncoderFactory> CreateCustomizedVideoEncoderFactory(Args args);

class CustomizedVideoEncoderFactory : public webrtc::VideoEncoderFactory {
public:
    CustomizedVideoEncoderFactory(Args args) : args_(args) {};
    ~CustomizedVideoEncoderFactory() {};

    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
        const webrtc::SdpVideoFormat &format) override;

private:
    Args args_;
};

#endif // CUSTOMIZED_VIDEO_ENCODER_FACTORY_H_
