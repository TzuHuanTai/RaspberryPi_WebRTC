#include "recorder/h264_recorder.h"

std::unique_ptr<H264Recorder> H264Recorder::Create(Args config) {
    auto ptr = std::make_unique<H264Recorder>(config, "h264_v4l2m2m");
    ptr->Initialize();
    ptr->ResetCodecs();
    return ptr;
}

H264Recorder::H264Recorder(Args config, std::string encoder_name)
    : VideoRecorder(config, encoder_name),
      is_ready_(false) {};

H264Recorder::~H264Recorder() {
    decoder_.reset();
    encoder_.reset();
}

void H264Recorder::Encode(Buffer buffer) {
    if (!is_ready_) {
        return;
    }
    decoder_->EmplaceBuffer(buffer, [this](Buffer decoded_buffer) {
        encoder_->EmplaceBuffer(decoded_buffer, [this](Buffer encoded_buffer) {
            OnEncoded(encoded_buffer);
        });
    });
}

void H264Recorder::ResetCodecs() {
    is_ready_ = false;
    decoder_ = std::make_unique<V4l2Decoder>();
    decoder_->Configure(config.width, config.height, V4L2_PIX_FMT_MJPEG, true);

    encoder_ = std::make_unique<V4l2Encoder>();
    encoder_->SetProfile(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
    encoder_->Configure(config.width, config.height, true);
    V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
    is_ready_ = true;
}
