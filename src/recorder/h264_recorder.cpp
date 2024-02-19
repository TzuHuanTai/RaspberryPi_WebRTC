#include "recorder/h264_recorder.h"

const int SECOND_PER_FILE = 30;

std::unique_ptr<H264Recorder> H264Recorder::Create(std::shared_ptr<V4L2Capture> capture) {
    auto ptr = std::make_unique<H264Recorder>(capture, "h264_v4l2m2m");
    ptr->SubscribeBufferSource();
    return std::move(ptr);
}

H264Recorder::H264Recorder(std::shared_ptr<V4L2Capture> capture, std::string encoder_name)
        : VideoRecorder(capture, encoder_name),
          frame_count_(0) {};

H264Recorder::~H264Recorder() {
    decoder_.reset();
    encoder_.reset();
}

void H264Recorder::Encode(Buffer buffer) {
    if (frame_count_++ % (SECOND_PER_FILE * config.fps) == 0) {
        FinishFile();
        InitializeContainer();
        AsyncWriteFileBackground();
        ResetCodecs();
    }

    decoder_->EmplaceBuffer(buffer, [&](Buffer decoded_buffer) {
        encoder_->EmplaceBuffer(decoded_buffer, [&](Buffer encoded_buffer) {
            PushEncodedBuffer(encoded_buffer);
        });
    });
}

void H264Recorder::ResetCodecs() {
    decoder_ = std::make_unique<V4l2Decoder>();
    decoder_->Configure(config.width, config.height, capture->format(), true);

    encoder_ = std::make_unique<V4l2Encoder>();
    encoder_->SetProfile(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
    encoder_->Configure(config.width, config.height, true);
    V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
}
