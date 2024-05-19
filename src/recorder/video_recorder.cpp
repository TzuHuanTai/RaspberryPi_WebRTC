#include "recorder/video_recorder.h"
#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"

VideoRecorder::VideoRecorder(Args config, std::string encoder_name)
    : Recorder(),
      encoder_name(encoder_name),
      config(config),
      wait_first_keyframe(false) {}

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

void VideoRecorder::OnBuffer(Buffer buffer) {
    Encode(buffer);
}

void VideoRecorder::PushBuffer(Buffer encoded_buffer) {
    if (raw_buffer_queue.size() < config.fps * 10) {
        Buffer buf = {.start = malloc(encoded_buffer.length),
                      .length = encoded_buffer.length,
                      .flags = encoded_buffer.flags};
        memcpy(buf.start, encoded_buffer.start, encoded_buffer.length);

        raw_buffer_queue.push(std::move(buf));
    }

    while (is_started && !raw_buffer_queue.empty()) {
        auto buffer = raw_buffer_queue.front();
        Write(buffer);
        free(buffer.start);
        raw_buffer_queue.pop();
    }
}

bool VideoRecorder::Write(Buffer buffer) {
    int ret;
    if (!wait_first_keyframe && (buffer.flags & V4L2_BUF_FLAG_KEYFRAME)) {
        wait_first_keyframe = true;
    }

    if (!wait_first_keyframe) {
        return false;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = static_cast<uint8_t *>(buffer.start);
    pkt.size = buffer.length;

    pkt.stream_index = st->index;
    pkt.pts = pkt.dts = av_rescale_q(frame_count++, encoder->time_base, st->time_base);
    OnEncoded(&pkt);
    av_packet_unref(&pkt);
    return true;
}
