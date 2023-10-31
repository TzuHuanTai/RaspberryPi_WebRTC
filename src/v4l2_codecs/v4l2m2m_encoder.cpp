#include "v4l2_codecs/v4l2m2m_encoder.h"

#include <sys/mman.h>
#include <iostream>
#include <memory>

V4l2m2mEncoder::V4l2m2mEncoder()
    : name_("h264_v4l2m2m"),
      is_recording_(false),
      recorder_(nullptr),
      callback_(nullptr),
      bitrate_adjuster_(.85, 1) {}

V4l2m2mEncoder::~V4l2m2mEncoder() {}

int32_t V4l2m2mEncoder::InitEncode(
    const webrtc::VideoCodec *codec_settings,
    const VideoEncoder::Settings &settings) {
    codec_ = *codec_settings;
    width_ = codec_settings->width;
    height_ = codec_settings->height;
    std::cout << "[V4l2m2mEncoder]: width, " << width_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: height, " << height_ << std::endl;

    encoded_image_.timing_.flags = webrtc::VideoSendTiming::TimingFrameFlags::kInvalid;
    encoded_image_.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

    if (codec_.codecType != webrtc::kVideoCodecH264) {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    encoder_ = std::make_unique<V4l2Encoder>();
    encoder_->Configure(width_, height_, false);

    EnableRecorder(is_recording_);

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4l2m2mEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback *callback) {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4l2m2mEncoder::Release() {
    std::lock_guard<std::mutex> lock(mtx_);
    recorder_.reset();
    encoder_.reset();
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4l2m2mEncoder::Encode(
    const webrtc::VideoFrame &frame,
    const std::vector<webrtc::VideoFrameType> *frame_types) {
    std::lock_guard<std::mutex> lock(mtx_);

    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
        frame.video_frame_buffer();

    auto i420_buffer = frame_buffer->GetI420();
    unsigned int i420_buffer_size = (i420_buffer->StrideY() * height_) +
                    ((i420_buffer->StrideY() + 1) / 2) * ((height_ + 1) / 2) * 2;

    Buffer src_buffer = {
        .start = const_cast<uint8_t*>(i420_buffer->DataY()),
        .length = i420_buffer_size
    };

    // skip sending task if output_result is false
    bool is_output = encoder_->EmplaceBuffer(src_buffer,
                [&](Buffer encoded_buffer) { 
                    SendFrame(frame, encoded_buffer);
                });

    return is_output? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

void V4l2m2mEncoder::SetRates(const RateControlParameters &parameters) {
    std::lock_guard<std::mutex> lock(mtx_);
    encoder_->SetFps(parameters, bitrate_adjuster_);
}

webrtc::VideoEncoder::EncoderInfo V4l2m2mEncoder::GetEncoderInfo() const {
    EncoderInfo info;
    info.supports_native_handle = true;
    info.implementation_name = name_;
    return info;
}

void V4l2m2mEncoder::EnableRecorder(bool onoff) {
    std::lock_guard<std::mutex> lock(recording_mtx_);
    is_recording_ = onoff;
    if (onoff) {
        recorder_.reset(new Recorder(args_));
    } else {
        recorder_.reset();
    }
}

void V4l2m2mEncoder::RegisterRecordingObserver(std::shared_ptr<Observable<char *>> observer,
                                               Args args) {
    observer_ = observer;
    args_ = args;
    args_.encoder_name = name_;

    // TODO: parse incoming message into object to control recoder
    observer_->Subscribe([&](char *message) {
        std::cout << "[V4l2m2mEncoder]: received msg => " << message << std::endl;
        if (strcmp(message, "true") == 0) {
            EnableRecorder(true);
            return;
        }
        EnableRecorder(false);
    });
}

void V4l2m2mEncoder::WriteFile(Buffer encoded_buffer) {
    std::lock_guard<std::mutex> lock(recording_mtx_);
    if (recorder_ && is_recording_ && encoded_buffer.length > 0) {
        recorder_->PushEncodedBuffer(encoded_buffer);
    }
}

void V4l2m2mEncoder::SendFrame(const webrtc::VideoFrame &frame, Buffer &encoded_buffer) {
    auto encoded_image_buffer =
        webrtc::EncodedImageBuffer::Create((uint8_t *)encoded_buffer.start, encoded_buffer.length);

    webrtc::CodecSpecificInfo codec_specific;
    codec_specific.codecType = webrtc::kVideoCodecH264;
    codec_specific.codecSpecific.H264.packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;

    encoded_image_.SetEncodedData(encoded_image_buffer);
    encoded_image_.SetTimestamp(frame.timestamp());
    encoded_image_.SetColorSpace(frame.color_space());
    encoded_image_._encodedWidth = width_;
    encoded_image_._encodedHeight = height_;
    encoded_image_.capture_time_ms_ = frame.render_time_ms();
    encoded_image_.ntp_time_ms_ = frame.ntp_time_ms();
    encoded_image_.rotation_ = frame.rotation();
    encoded_image_._frameType = webrtc::VideoFrameType::kVideoFrameDelta;

    if (encoded_buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
        encoded_image_._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    }

    WriteFile(encoded_buffer);

    auto result = callback_->OnEncodedImage(encoded_image_, &codec_specific);
    if (result.error != webrtc::EncodedImageCallback::Result::OK) {
        std::cout << "OnEncodedImage failed error:" << result.error << std::endl;
    }
}
