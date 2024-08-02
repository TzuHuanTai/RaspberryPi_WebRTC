#include "recorder/video_recorder.h"

#include <memory>

#include "common/utils.h"
#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"
#include "v4l2_codecs/raw_buffer.h"

VideoRecorder::VideoRecorder(Args config, std::string encoder_name)
    : Recorder(),
      encoder_name(encoder_name),
      config(config),
      feeded_frames(0),
      has_first_keyframe(false) {}

void VideoRecorder::InitializeEncoderCtx(AVCodecContext *&encoder) {
    frame_rate = {.num = (int)config.fps, .den = 1};

    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    encoder = avcodec_alloc_context3(codec);
    encoder->codec_type = AVMEDIA_TYPE_VIDEO;
    encoder->width = config.width;
    encoder->height = config.height;
    encoder->framerate = frame_rate;
    encoder->time_base = av_inv_q(frame_rate);
    encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

void VideoRecorder::OnBuffer(rtc::scoped_refptr<V4l2FrameBuffer> &buffer) {
    if (frame_buffer_queue.size() < config.fps * 10) {
        frame_buffer_queue.push(buffer);
    }
}

void VideoRecorder::PostStop() {
    // Wait P-frames are all consumed until I-frame appear.
    while (!frame_buffer_queue.empty() &&
           (frame_buffer_queue.front()->flags() & V4L2_BUF_FLAG_KEYFRAME) != 0) {
        ConsumeBuffer();
    }
    feeded_frames = 0;
    has_first_keyframe = false;
}

void VideoRecorder::SetBaseTimestamp(struct timeval time) { base_time_ = time; }

void VideoRecorder::OnEncoded(V4l2Buffer &buffer) {
    if (!has_first_keyframe && (buffer.flags & V4L2_BUF_FLAG_KEYFRAME)) {
        has_first_keyframe = true;
        SetBaseTimestamp(buffer.timestamp);
    }

    if (!has_first_keyframe) {
        return;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = static_cast<uint8_t *>(buffer.start);
    pkt.size = buffer.length;
    pkt.stream_index = st->index;

    double elapsed_time = (buffer.timestamp.tv_sec - base_time_.tv_sec) +
                          (buffer.timestamp.tv_usec - base_time_.tv_usec) / 1000000.0;
    pkt.pts = pkt.dts = static_cast<int>(elapsed_time * st->time_base.den / st->time_base.num);

    OnPacketed(&pkt);
    av_packet_unref(&pkt);
}

bool VideoRecorder::ConsumeBuffer() {
    if (frame_buffer_queue.empty()) {
        return false;
    }
    auto frame_buffer = frame_buffer_queue.front();

    V4l2Buffer buffer((void *)frame_buffer->Data(), frame_buffer->size(), frame_buffer->flags(),
                      frame_buffer->timestamp());

    Encode(buffer);

    if (feeded_frames >= 0) {
        MakePreviewImage(buffer);
    } else if (feeded_frames < 0 && image_decoder_->IsCapturing()) {
        image_decoder_->ReleaseCodec();
    }

    frame_buffer_queue.pop();

    return true;
}

void VideoRecorder::MakePreviewImage(V4l2Buffer &buffer) {
    if (feeded_frames == 0 || buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
        image_decoder_ = std::make_unique<V4l2Decoder>();
        image_decoder_->Configure(config.width, config.height, V4L2_PIX_FMT_H264, false);
        image_decoder_->Start();
    }

    if (feeded_frames >= 0) {
        feeded_frames++;
        image_decoder_->EmplaceBuffer(buffer, [this](V4l2Buffer decoded_buffer) {
            if (feeded_frames < 0) {
                return;
            }
            auto raw_buffer = RawBuffer::Create(config.width, config.height, decoded_buffer.length,
                                                decoded_buffer);
            int dst_stride = config.width / 2;
            auto i420buff = webrtc::I420Buffer::Create(config.width / 2, config.height / 2, dst_stride,
                                                 dst_stride / 2, dst_stride / 2);
            i420buff->ScaleFrom(*raw_buffer->ToI420());
            Utils::CreateJpegImage(i420buff->DataY(), i420buff->width(), i420buff->height(),
                                   ReplaceExtension(file_url, ".jpg"));
            feeded_frames = -1;
        });
    }
}

std::string VideoRecorder::ReplaceExtension(const std::string &url,
                                            const std::string &new_extension) {
    size_t last_dot_pos = url.find_last_of('.');
    if (last_dot_pos == std::string::npos) {
        // No extension found, append the new extension
        return url + new_extension;
    } else {
        // Replace the existing extension
        return url.substr(0, last_dot_pos) + new_extension;
    }
}
