#include "customized_video_encoder_factory.h"
#include "encoder/raw_buffer_encoder.h"
#include "encoder/v4l2m2m_encoder.h"

#include <media/base/codec.h>

#include <iostream>

std::unique_ptr<webrtc::VideoEncoderFactory> CreateCustomizedVideoEncoderFactory()
{
    return std::make_unique<CustomizedVideoEncoderFactory>();
}

std::vector<webrtc::SdpVideoFormat>
CustomizedVideoEncoderFactory::GetSupportedFormats() const
{
    std::vector<webrtc::SdpVideoFormat> supported_codecs = {
        CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                         webrtc::H264Level::kLevel3_1, "1"),
        CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                         webrtc::H264Level::kLevel3_1, "0")};

    return supported_codecs;
}

std::unique_ptr<webrtc::VideoEncoder>
CustomizedVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat &format)
{
    std::cout << "[CustomizedVideoEncoderFactory]: CreateVideoEncoder, " << format.name << std::endl;
    // if (format.name == cricket::kH264CodecName)
    // {
    // return std::unique_ptr<webrtc::VideoEncoder>(absl::make_unique<RawBufferEncoder>(format));
    return std::unique_ptr<webrtc::VideoEncoder>(absl::make_unique<V4l2m2mEncoder>());
    // }
}
