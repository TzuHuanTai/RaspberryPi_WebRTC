#include "recorder/video_recorder.h"
#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"

#include <memory>

VideoRecorder::VideoRecorder(Args config, std::string encoder_name)
    : Recorder(),
      encoder_name(encoder_name),
      config(config),
      has_first_keyframe(false) {}

AVCodecContext* VideoRecorder::InitializeEncoderCtx() {
    frame_rate = {.num = (int)config.fps, .den = 1};

    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    auto encoder = avcodec_alloc_context3(codec);
    encoder->codec_type = AVMEDIA_TYPE_VIDEO;
    encoder->width = config.width;
    encoder->height = config.height;
    encoder->framerate = frame_rate;
    encoder->time_base = av_inv_q(frame_rate);
    encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    return encoder;
}

void VideoRecorder::OnBuffer(Buffer &raw_buffer) {
    if (raw_buffer_queue.size() < config.fps * 10) {
        Buffer buf = {.start = malloc(raw_buffer.length),
                      .length = raw_buffer.length,
                      .flags = raw_buffer.flags,
                      .timestamp = raw_buffer.timestamp};
        memcpy(buf.start, raw_buffer.start, raw_buffer.length); 
        raw_buffer_queue.push(std::move(buf));
    }
}

void VideoRecorder::PostStop() {
    // Wait P-frames are all consumed until I-frame appear.
    while (!raw_buffer_queue.empty() && 
           !(raw_buffer_queue.front().flags & V4L2_BUF_FLAG_KEYFRAME)) {
        ConsumeBuffer();
    }

    has_first_keyframe = false;
}

void VideoRecorder::SetBaseTimestamp(struct timeval time) {
    base_time_ = time;
}

void VideoRecorder::OnEncoded(Buffer buffer) {
    int ret;
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
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (raw_buffer_queue.empty()) {
        return false;
    }
    auto buffer = std::move(raw_buffer_queue.front());
    Encode(buffer);
    if (buffer.start != nullptr) {
        free(buffer.start);
        buffer.start = nullptr;
    }
    raw_buffer_queue.pop();
    return true;
}
