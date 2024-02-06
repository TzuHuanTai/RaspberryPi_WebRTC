#include "customized_video_encoder_factory.h"
#include "v4l2_codecs/v4l2_h264_encoder.h"

#if WEBRTC_USE_H264
#include <modules/video_coding/codecs/h264/include/h264.h>
#endif
#include <modules/video_coding/codecs/vp8/include/vp8.h>
#include <modules/video_coding/codecs/vp9/include/vp9.h>
#include <modules/video_coding/codecs/av1/av1_svc_config.h>
#include <modules/video_coding/codecs/av1/libaom_av1_encoder.h>

std::unique_ptr<webrtc::VideoEncoderFactory> CreateCustomizedVideoEncoderFactory(Args args) {
    return std::make_unique<CustomizedVideoEncoderFactory>(args);
}

std::vector<webrtc::SdpVideoFormat>
CustomizedVideoEncoderFactory::GetSupportedFormats() const {
    std::vector<webrtc::SdpVideoFormat> supported_codecs;

    // vp8
    supported_codecs.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));

    // vp9
    auto supported_vp9_formats = webrtc::SupportedVP9Codecs(true);
    supported_codecs.insert(supported_codecs.end(), 
                            std::begin(supported_vp9_formats),
                            std::end(supported_vp9_formats));

    // av1
    supported_codecs.push_back(webrtc::SdpVideoFormat(
        cricket::kAv1CodecName, webrtc::SdpVideoFormat::Parameters(),
        webrtc::LibaomAv1EncoderSupportedScalabilityModes()));
    // h264
#if WEBRTC_USE_H264
    auto supported_h264_formats = webrtc::SupportedH264Codecs(true);
    supported_codecs.insert(supported_codecs.end(), 
                            std::begin(supported_h264_formats),
                            std::end(supported_h264_formats));
#else
    supported_codecs.push_back(CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                               webrtc::H264Level::kLevel3_1, "1"));
    supported_codecs.push_back(CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                               webrtc::H264Level::kLevel3_1, "0"));
#endif
    return supported_codecs;
}

std::unique_ptr<webrtc::VideoEncoder>
CustomizedVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat &format) {
    if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName)) {
#if USE_BUILT_IN_H264
        return webrtc::H264Encoder::Create(cricket::VideoCodec(format));
#else
        return V4l2H264Encoder::Create(args_.enable_v4l2_dma);
#endif
    } else if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName)) {
        return webrtc::VP8Encoder::Create();
    } else if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName)) {
        return webrtc::VP9Encoder::Create(cricket::VideoCodec(format));
    } else if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName)) {
        return webrtc::CreateLibaomAv1Encoder();
    }

    return nullptr;
}
