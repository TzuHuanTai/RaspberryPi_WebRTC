#include "v4l2_codecs/raw_buffer_encoder.h"
#include "v4l2_codecs/raw_buffer.h"

#include <future>

RawBufferEncoder::RawBufferEncoder() {}

RawBufferEncoder::~RawBufferEncoder() {}

int32_t RawBufferEncoder::InitEncode(
    const webrtc::VideoCodec *codec_settings,
    const VideoEncoder::Settings &settings) {
    codec_ = *codec_settings;
    width_ = codec_settings->width;
    height_ = codec_settings->height;
    bitrate_adjuster_.SetTargetBitrateBps(codec_settings->startBitrate * 1000);

    if (codec_.codecType != webrtc::kVideoCodecH264) {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    encoded_image_.timing_.flags = webrtc::VideoSendTiming::TimingFrameFlags::kInvalid;
    encoded_image_.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

    auto e = std::async(std::launch::async, [&]() {
        encoder_ = std::make_unique<V4l2Encoder>();
        encoder_->Configure(width_, height_, true);
    });

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RawBufferEncoder::Encode(
    const webrtc::VideoFrame &frame,
    const std::vector<webrtc::VideoFrameType> *frame_types) {
    if (frame_types &&  (*frame_types)[0] == webrtc::VideoFrameType::kVideoFrameKey) {
        V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
    }

    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
        frame.video_frame_buffer();

    if (frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    RawBuffer *raw_buffer = static_cast<RawBuffer *>(frame_buffer.get());
    Buffer buffer = raw_buffer->GetBuffer();

    // skip sending task if output_result is false
    bool is_output = encoder_->EmplaceBuffer(buffer,
        [&](Buffer encoded_buffer) {
            SendFrame(frame, encoded_buffer);
        });
    
    return is_output? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

webrtc::VideoEncoder::EncoderInfo RawBufferEncoder::GetEncoderInfo() const {
    EncoderInfo info;
    info.supports_native_handle = true;
    info.implementation_name = "Raw H264";
    return info;
}
