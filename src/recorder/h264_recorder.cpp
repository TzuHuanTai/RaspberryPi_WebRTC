#include "recorder/h264_recorder.h"

std::unique_ptr<H264Recorder> H264Recorder::Create(
    std::shared_ptr<V4L2Capture> capture) {
    auto ptr = std::make_unique<H264Recorder>(capture, "h264_v4l2m2m");
    ptr->Initialize();
    return ptr;
}

H264Recorder::H264Recorder(
    std::shared_ptr<V4L2Capture> capture, std::string encoder_name)
    : VideoRecorder(capture, encoder_name) {};

H264Recorder::~H264Recorder() {
    decoder_.reset();
    encoder_.reset();
}

void H264Recorder::Encode(Buffer buffer) {
    if (frame_count_ == 0) {
        ResetCodecs();
    }

    decoder_->EmplaceBuffer(buffer, [&](Buffer decoded_buffer) {
        encoder_->EmplaceBuffer(decoded_buffer, [&](Buffer encoded_buffer) {
            PushBuffer(encoded_buffer);
        });
    });
}

void H264Recorder::ResetCodecs() {
    decoder_ = std::make_unique<V4l2Decoder>();
    decoder_->Configure(config.width, config.height, V4L2_PIX_FMT_H264, true);

    encoder_ = std::make_unique<V4l2Encoder>();
    encoder_->SetProfile(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
    encoder_->Configure(config.width, config.height, true);
    V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
}
