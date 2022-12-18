#include "recorder.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <unistd.h>

Recorder::Recorder(RecorderConfig config)
    : width(config.width),
      height(config.height),
      base_path(config.saving_path),
      extension(config.container),
      encoder_name(config.encoder_name),
      frame_rate({.num = (int)config.fps, .den = 1}),
      buffer_limit_num_(config.fps * 10),
      video_frame_count_(0),
      wait_first_keyframe_(false)
{
    Initialize();
    AsyncWriteFileBackground();
}

std::string Recorder::PrefixZero(int src, int digits)
{
    std::string str = std::to_string(src);
    std::string n_zero(digits - str.length(), '0');
    return n_zero + str;
}

std::string Recorder::GenerateFilename()
{
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

bool Recorder::Initialize()
{
    filename = GenerateFilename() + "." + extension;
    full_path = base_path + '/' + filename;

    if (avformat_alloc_output_context2(&fmt_ctx_, nullptr,
                                       extension.c_str(),
                                       full_path.c_str()) < 0)
    {
        fprintf(stderr, "Could not alloc output context");
        return false;
    }

    AddVideoStream();

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&fmt_ctx_->pb, full_path.c_str(), AVIO_FLAG_WRITE) < 0)
        {
            fprintf(stderr, "Could not open '%s'\n", full_path.c_str());
            return false;
        }
    }

    av_dump_format(fmt_ctx_, 0, full_path.c_str(), 1);

    if (avformat_write_header(fmt_ctx_, nullptr) < 0)
    {
        fprintf(stderr, "Error occurred when opening output file\n");
        return false;
    }

    return true;
}

void Recorder::AddVideoStream()
{
    // encoder just for setting up stream->codecpar.
    AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    video_encoder_ = avcodec_alloc_context3(codec);
    video_encoder_->codec_type = AVMEDIA_TYPE_VIDEO;
    video_encoder_->width = 1280;
    video_encoder_->height = 720;
    video_encoder_->framerate = frame_rate;
    video_encoder_->time_base = av_inv_q(frame_rate);
    video_encoder_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    video_st_ = avformat_new_stream(fmt_ctx_, 0);
    video_st_->avg_frame_rate = frame_rate;
    video_st_->r_frame_rate = frame_rate;
    avcodec_parameters_from_context(video_st_->codecpar, video_encoder_);
}

void Recorder::PushBuffer(Buffer buffer)
{
    if (buffer_queue_.size() < buffer_limit_num_)
    {
        Buffer buf = {
            .start = malloc(buffer.length),
            .length = buffer.length,
            .flags = buffer.flags};
        memcpy(buf.start, buffer.start, buffer.length);

        buffer_queue_.push(buf);
    }
}

void Recorder::ConsumeBuffer()
{
    while (is_recording_)
    {
        while (!buffer_queue_.empty())
        {
            Write(buffer_queue_.front());
            buffer_queue_.pop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Recorder::AsyncWriteFileBackground()
{
    is_recording_ = true;
    consumer_ = std::async(std::launch::async, &Recorder::ConsumeBuffer, this);
}

bool Recorder::Write(Buffer buffer)
{
    if (!wait_first_keyframe_ && (buffer.flags & V4L2_BUF_FLAG_KEYFRAME))
    {
        wait_first_keyframe_ = true;
    }

    if (!wait_first_keyframe_)
    {
        return false;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = static_cast<uint8_t *>(buffer.start);
    pkt.size = buffer.length;

    pkt.stream_index = video_st_->index;
    pkt.pts = pkt.dts = av_rescale_q(video_frame_count_++, av_inv_q(frame_rate), video_st_->time_base);

    if (av_interleaved_write_frame(fmt_ctx_, &pkt) < 0)
    {
        std::cout << "av_interleaved_write_frame: error" << std::endl;
        return false;
    }

    return true;
}

void Recorder::Finish()
{
    is_recording_ = false;

    consumer_.wait();

    if (fmt_ctx_)
    {
        av_write_trailer(fmt_ctx_);
        avio_closep(&fmt_ctx_->pb);
        avformat_free_context(fmt_ctx_);
        std::cout << "[Recorder]: finished" << std::endl;
    }
}

Recorder::~Recorder()
{
    Finish();
}
