#include "customized_video_encoder_factory.h"
#include "encoder/v4l2m2m_encoder.h"
#include "encoder/raw_buffer_encoder.h"

#include <absl/strings/match.h>
#include <api/video_codecs/sdp_video_format.h>
#include <api/video_codecs/video_encoder.h>
#include <media/base/codec.h>
#include <media/base/media_constants.h>
#include <media/engine/encoder_simulcast_proxy.h>
#include <media/engine/internal_encoder_factory.h>

std::unique_ptr<webrtc::VideoEncoderFactory> CreateCustomizedVideoEncoderFactory(
    Args args, std::shared_ptr<DataChannelSubject> data_channel_subject)
{
    return std::make_unique<CustomizedVideoEncoderFactory>(args, data_channel_subject);
}

std::vector<webrtc::SdpVideoFormat>
CustomizedVideoEncoderFactory::GetSupportedFormats() const
{
    // auto supported_codecs = internal_encoder_factory_->GetSupportedFormats();
    std::vector<webrtc::SdpVideoFormat> supported_codecs;
    supported_codecs.push_back(CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                         webrtc::H264Level::kLevel3_1, "1"));
    supported_codecs.push_back(CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                         webrtc::H264Level::kLevel3_1, "0"));
    return supported_codecs;
}

std::unique_ptr<webrtc::VideoEncoder>
CustomizedVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat &format)
{
    // if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName))
    // {
    //     auto observer = data_channel_subject_->AsObservable();
    //     auto encoder = std::make_unique<V4l2m2mEncoder>();
    //     encoder->RegisterRecordingObserver(observer, args_.file_path);

    //     return std::unique_ptr<webrtc::VideoEncoder>(std::move(encoder));
    // }
    // else
    // {
    //     std::unique_ptr<webrtc::VideoEncoder> internal_encoder;
    //     if (format.IsCodecInList(internal_encoder_factory_->GetSupportedFormats())) {
    //         internal_encoder = std::make_unique<webrtc::EncoderSimulcastProxy>(
    //             internal_encoder_factory_.get(), format);
    //     }

    //     return internal_encoder;
    // }

    return std::unique_ptr<webrtc::VideoEncoder>(std::make_unique<RawBufferEncoder>(format));
}
