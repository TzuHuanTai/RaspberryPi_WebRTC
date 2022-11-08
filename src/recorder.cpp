#include "recorder.h"

#include <iostream>
#include <sstream>

Recorder::Recorder(Args args) : width(args.width),
                                height(args.height),
                                base_path(args.file_path),
                                encoder_name(args.packet_type),
                                frame_rate({.num = (int)args.fps, .den = 1}),
                                video_frame_count_(0)
{
    CreateFormatContext(args.container.c_str());
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

void Recorder::CreateFormatContext(const char *extension)
{
    filename = GenerateFilename() + "." + extension;
    full_path = base_path + '/' + filename;

    if (avformat_alloc_output_context2(&fmt_ctx_, nullptr, extension, full_path.c_str()) < 0)
    {
        fprintf(stderr, "Could not alloc output context");
        exit(1);
    }

    AddVideoStream();

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&fmt_ctx_->pb, full_path.c_str(), AVIO_FLAG_WRITE) < 0)
        {
            fprintf(stderr, "Could not open '%s'\n", full_path.c_str());
            exit(1);
        }
    }

    av_dump_format(fmt_ctx_, 0, full_path.c_str(), 1);

    if (avformat_write_header(fmt_ctx_, nullptr) < 0)
    {
        fprintf(stderr, "Error occurred when opening output file\n");
        exit(1);
    }
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

void Recorder::Write(Buffer buffer)
{
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = static_cast<uint8_t *>(buffer.start);
    pkt.size = buffer.length;

    pkt.stream_index = video_st_->index;
    pkt.pts = pkt.dts = av_rescale_q(video_frame_count_++, av_inv_q(frame_rate), video_st_->time_base);

    if (av_interleaved_write_frame(fmt_ctx_, &pkt) < 0)
    {
        std::cout << "av_interleaved_write_frame: error" << std::endl;
    }
}

void Recorder::Finish()
{
    av_write_trailer(fmt_ctx_);
    avio_closep(&fmt_ctx_->pb);
    avformat_free_context(fmt_ctx_);
}

Recorder::~Recorder()
{
    std::cout << "~Recorder" << std::endl;
    Finish();
}
