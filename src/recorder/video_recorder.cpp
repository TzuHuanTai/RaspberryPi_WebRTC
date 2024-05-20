#include "recorder/video_recorder.h"
#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"

#include <unistd.h>

VideoRecorder::VideoRecorder(Args config, std::string encoder_name)
    : Recorder(),
      encoder_name(encoder_name),
      config(config),
      wait_first_keyframe(false) {}

void VideoRecorder::Initialize() {
    InitializeEncoder();
    worker_.reset(new Worker([this]() { 
        if (is_started && !raw_buffer_queue.empty()) {
            auto buffer = raw_buffer_queue.front();
            Encode(buffer);
            free(buffer.start);
            raw_buffer_queue.pop();
        } else {
            usleep(10);
        }
    }));
    worker_->Run();
}

void VideoRecorder::InitializeEncoder() {
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

void VideoRecorder::OnBuffer(Buffer raw_buffer) {
    if (raw_buffer_queue.size() < config.fps * 10) {
        Buffer buf = {.start = malloc(raw_buffer.length),
                      .length = raw_buffer.length,
                      .flags = raw_buffer.flags};
        memcpy(buf.start, raw_buffer.start, raw_buffer.length); 
        raw_buffer_queue.push(std::move(buf));
    }
}

void VideoRecorder::OnEncoded(Buffer buffer) {
    int ret;
    if (!wait_first_keyframe && (buffer.flags & V4L2_BUF_FLAG_KEYFRAME)) {
        wait_first_keyframe = true;
    }

    if (!wait_first_keyframe) {
        return;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = static_cast<uint8_t *>(buffer.start);
    pkt.size = buffer.length;

    pkt.stream_index = st->index;
    pkt.pts = pkt.dts = av_rescale_q(frame_count++, encoder->time_base, st->time_base);
    OnPacketed(&pkt);
    av_packet_unref(&pkt);
}
