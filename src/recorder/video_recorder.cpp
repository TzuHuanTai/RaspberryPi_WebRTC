#include "recorder/video_recorder.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <unistd.h>

VideoRecorder::VideoRecorder(std::shared_ptr<V4L2Capture> capture, std::string encoder_name)
    : encoder_name(encoder_name),
      config(capture->config()),
      video_frame_count(0),
      wait_first_keyframe(false) {}

std::string VideoRecorder::PrefixZero(int src, int digits) {
    std::string str = std::to_string(src);
    std::string n_zero(digits - str.length(), '0');
    return n_zero + str;
}

std::string VideoRecorder::GenerateFilename() {
    time_t now = time(0);
    tm *ltm = localtime(&now);

    std::string year = PrefixZero(1900 + ltm->tm_year, 4);
    std::string month = PrefixZero(1 + ltm->tm_mon, 2);
    std::string day = PrefixZero(ltm->tm_mday, 2);
    std::string hour = PrefixZero(ltm->tm_hour, 2);
    std::string min = PrefixZero(ltm->tm_min, 2);
    std::string sec = PrefixZero(ltm->tm_sec, 2);

    std::stringstream s1;
    std::string filename;
    s1 << year << month << day << "_" << hour << min << sec;
    s1 >> filename;

    return filename;
}

bool VideoRecorder::Initialize() {
    std::string container = "";
    if (encoder_name == "libvpx") {
        container = "webm";
    } else {
        container = "mp4";
    }
    auto filename = GenerateFilename() + "." + container;
    auto full_path = config.record_path + '/' + filename;

    if (avformat_alloc_output_context2(&fmt_ctx, nullptr,
                                       container.c_str(),
                                       full_path.c_str()) < 0) {
        fprintf(stderr, "Could not alloc output context");
        return false;
    }

    AddVideoStream();

    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, full_path.c_str(), AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open '%s'\n", full_path.c_str());
            return false;
        }
    }

    av_dump_format(fmt_ctx, 0, full_path.c_str(), 1);

    if (avformat_write_header(fmt_ctx, nullptr) < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        return false;
    }

    return true;
}

void VideoRecorder::AddVideoStream() {
    frame_rate = {.num = (int)config.fps, .den = 1};

    // encoder just for setting up stream->codecpar.
    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    video_encoder = avcodec_alloc_context3(codec);
    video_encoder->codec_type = AVMEDIA_TYPE_VIDEO;
    video_encoder->width = config.width;
    video_encoder->height = config.height;
    video_encoder->framerate = frame_rate;
    video_encoder->time_base = av_inv_q(frame_rate);
    video_encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    video_st = avformat_new_stream(fmt_ctx, 0);
    video_st->avg_frame_rate = frame_rate;
    video_st->r_frame_rate = frame_rate;
    avcodec_parameters_from_context(video_st->codecpar, video_encoder);
}

void VideoRecorder::PushEncodedBuffer(Buffer encoded_buffer) {
    if (buffer_queue.size() < config.fps * 10) {
        Buffer buf = {.start = malloc(encoded_buffer.length),
                      .length = encoded_buffer.length,
                      .flags = encoded_buffer.flags};
        memcpy(buf.start, encoded_buffer.start, encoded_buffer.length);

        buffer_queue.push(buf);
    }
}

void VideoRecorder::ConsumeBuffer() {
    while (is_recording) {
        while (!buffer_queue.empty()) {
            Write(buffer_queue.front());
            buffer_queue.pop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void VideoRecorder::AsyncWriteFileBackground() {
    is_recording = true;
    consumer = std::async(std::launch::async, &VideoRecorder::ConsumeBuffer, this);
}

bool VideoRecorder::Write(Buffer buffer) {
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

    pkt.stream_index = video_st->index;
    pkt.pts = pkt.dts = av_rescale_q(video_frame_count++, av_inv_q(frame_rate), video_st->time_base);

    if (av_interleaved_write_frame(fmt_ctx, &pkt) < 0) {
        std::cout << "av_interleaved_write_frame: error" << std::endl;
        return false;
    }

    return true;
}

void VideoRecorder::Finish() {
    is_recording = false;

    consumer.wait();

    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
        avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
        std::cout << "[VideoRecorder]: finished" << std::endl;
    }
}

VideoRecorder::~VideoRecorder() {
    Finish();
}
