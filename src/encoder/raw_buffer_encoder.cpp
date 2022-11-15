#include "raw_buffer_encoder.h"
#include "raw_buffer.h"

#include <iostream>

RawBufferEncoder::RawBufferEncoder(const webrtc::SdpVideoFormat &format)
    : callback_(nullptr)
{
    bitrate_adjuster_.reset(new webrtc::BitrateAdjuster(.5, .95));
    std::cout << "[RawBufferEncoder]: constructor" << std::endl;
}

RawBufferEncoder::~RawBufferEncoder()
{
    Release();
}

int32_t RawBufferEncoder::InitEncode(
    const webrtc::VideoCodec *codec_settings,
    const VideoEncoder::Settings &settings)
{
    codec_ = *codec_settings;

    encoded_image_.timing_.flags = webrtc::VideoSendTiming::TimingFrameFlags::kInvalid;
    encoded_image_.content_type_ = webrtc::VideoContentType::UNSPECIFIED;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RawBufferEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback *callback)
{
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RawBufferEncoder::Release()
{
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RawBufferEncoder::Encode(
    const webrtc::VideoFrame &frame,
    const std::vector<webrtc::VideoFrameType> *frame_types)
{
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
        frame.video_frame_buffer();

    if (frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative)
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (codec_.codecType != webrtc::kVideoCodecH264)
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    RawBuffer *raw_buffer = static_cast<RawBuffer *>(frame_buffer.get());
    width_ = raw_buffer->width();
    height_ = raw_buffer->height();

    webrtc::CodecSpecificInfo codec_specific;
    codec_specific.codecType = webrtc::kVideoCodecH264;
    codec_specific.codecSpecific.H264.packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;

    auto encoded_image_buffer =
        webrtc::EncodedImageBuffer::Create(raw_buffer->Data(), raw_buffer->Size());
    encoded_image_.SetEncodedData(encoded_image_buffer);
    encoded_image_.SetTimestamp(frame.timestamp());
    encoded_image_.SetColorSpace(frame.color_space());
    encoded_image_._encodedWidth = width_;
    encoded_image_._encodedHeight = height_;
    encoded_image_.capture_time_ms_ = frame.render_time_ms();
    encoded_image_.ntp_time_ms_ = frame.ntp_time_ms();
    encoded_image_.rotation_ = frame.rotation();
    encoded_image_._frameType = webrtc::VideoFrameType::kVideoFrameDelta;

    if (raw_buffer->GetFlags() & V4L2_BUF_FLAG_KEYFRAME)
    {
        encoded_image_._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    }

    auto result = callback_->OnEncodedImage(encoded_image_, &codec_specific);
    if (result.error != webrtc::EncodedImageCallback::Result::OK)
    {
        std::cout << "OnEncodedImage failed error:" << result.error << std::endl;
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    bitrate_adjuster_->Update(raw_buffer->Size());

    return WEBRTC_VIDEO_CODEC_OK;
}

void RawBufferEncoder::SetRates(const RateControlParameters &parameters)
{
    if (parameters.bitrate.get_sum_bps() <= 0 || parameters.framerate_fps <= 0)
        return;

    std::cout << __FUNCTION__ << " framerate:" << parameters.framerate_fps
              << " bitrate:" << parameters.bitrate.ToString() << std::endl;
    framerate_ = parameters.framerate_fps;
    target_bitrate_bps_ = parameters.bitrate.get_sum_bps();

    bitrate_adjuster_->SetTargetBitrateBps(target_bitrate_bps_);
    return;
}

webrtc::VideoEncoder::EncoderInfo RawBufferEncoder::GetEncoderInfo() const
{
    EncoderInfo info;
    info.supports_native_handle = true;
    info.implementation_name = "Raw H264";
    return info;
}
