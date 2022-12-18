#include "customized_video_encoder_factory.h"
#include "encoder/raw_buffer_encoder.h"
#include "encoder/v4l2m2m_encoder.h"

#include <media/base/codec.h>

std::unique_ptr<webrtc::VideoEncoderFactory> CreateCustomizedVideoEncoderFactory(
    Args args, std::shared_ptr<DataChannelSubject> data_channel_subject)
{
    return std::make_unique<CustomizedVideoEncoderFactory>(args, data_channel_subject);
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
    auto observer = data_channel_subject_->AsObservable();
    auto encoder = std::make_unique<V4l2m2mEncoder>();
    encoder->RegisterRecordingObserver(observer, args_.file_path);

    return std::unique_ptr<webrtc::VideoEncoder>(std::move(encoder));
}
