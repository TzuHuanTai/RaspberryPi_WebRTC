#include "customized_video_encoder_factory.h"
#include "encoder/v4l2m2m_encoder.h"
#include "encoder/raw_buffer_encoder.h"

#include <absl/strings/match.h>
#include <api/video_codecs/sdp_video_format.h>
#include <api/video_codecs/video_encoder.h>
#include <media/base/codec.h>
#include <media/base/media_constants.h>
#include <media/engine/internal_encoder_factory.h>
#include <modules/video_coding/codecs/vp8/include/vp8.h>
#include <modules/video_coding/codecs/vp9/include/vp9.h>
#include <modules/video_coding/codecs/av1/av1_svc_config.h>
#include <modules/video_coding/codecs/av1/libaom_av1_encoder.h>

std::unique_ptr<webrtc::VideoEncoderFactory> CreateCustomizedVideoEncoderFactory(
    Args args, std::shared_ptr<DataChannelSubject> data_channel_subject)
{
    return std::make_unique<CustomizedVideoEncoderFactory>(args, data_channel_subject);
}

std::vector<webrtc::SdpVideoFormat>
CustomizedVideoEncoderFactory::GetSupportedFormats() const
{
    std::vector<webrtc::SdpVideoFormat> supported_codecs;

    // vp8
    supported_codecs.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));

    // vp9
    for (const webrtc::SdpVideoFormat& format :
         webrtc::SupportedVP9Codecs(true)) {
      supported_codecs.push_back(format);
    }

    // av1
    supported_codecs.push_back(webrtc::SdpVideoFormat(
        cricket::kAv1CodecName, webrtc::SdpVideoFormat::Parameters(),
        webrtc::LibaomAv1EncoderSupportedScalabilityModes()));

    // h264
    supported_codecs.push_back(CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                         webrtc::H264Level::kLevel3_1, "1"));
    supported_codecs.push_back(CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                         webrtc::H264Level::kLevel3_1, "0"));
    return supported_codecs;
}

std::unique_ptr<webrtc::VideoEncoder>
CustomizedVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat &format)
{
    if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName))
    {
        auto formats = V4l2Util::GetDeviceSupportedFormats(args_.device.c_str());
        if (absl::EqualsIgnoreCase(args_.v4l2_format, cricket::kH264CodecName)
            && formats.find(cricket::kH264CodecName) != formats.end())
        {
            return std::unique_ptr<webrtc::VideoEncoder>(std::make_unique<RawBufferEncoder>(format, args_));
        }
        else
        {
            auto observer = data_channel_subject_->AsObservable(CommandType::RECORD);
            auto encoder = std::make_unique<V4l2m2mEncoder>();
            encoder->RegisterRecordingObserver(observer, args_);
            return std::unique_ptr<webrtc::VideoEncoder>(std::move(encoder));
        }
    }
    else if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName))
    {
        return webrtc::VP8Encoder::Create();
    }
    else if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName))
    {
        return webrtc::VP9Encoder::Create(cricket::VideoCodec(format));
    }
    else if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName))
    {
        return webrtc::CreateLibaomAv1Encoder();
    }

    return nullptr;
}
