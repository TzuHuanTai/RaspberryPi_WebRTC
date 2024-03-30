#include "recorder/audio_recorder.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <math.h>
#include <unistd.h>

std::unique_ptr<AudioRecorder> AudioRecorder::CreateRecorder(std::string record_path) {
    auto p = std::make_unique<AudioRecorder>(record_path);
    p->InitializeContainer();
    p->InitializeFrame();
    p->InitializeFifoBuffer();
    return p;
}

AudioRecorder::AudioRecorder(std::string record_path)
    : encoder_name("aac"),
      record_path(record_path),
      frame_count(0) { }

std::string AudioRecorder::PrefixZero(int src, int digits) {
    std::string str = std::to_string(src);
    std::string n_zero(digits - str.length(), '0');
    return n_zero + str;
}

std::string AudioRecorder::GenerateFilename() {
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

bool AudioRecorder::InitializeContainer() {
    std::string container = "mp4";
    auto filename = GenerateFilename() + "." + container;
    auto full_path = record_path + '/' + filename;

    if (avformat_alloc_output_context2(&fmt_ctx, nullptr,
                                       container.c_str(),
                                       full_path.c_str()) < 0) {
        fprintf(stderr, "Could not alloc output context");
        return false;
    }

    AddAudioStream();

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

void AudioRecorder::AddAudioStream() {
    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    encoder = avcodec_alloc_context3(codec);
    encoder->codec_type = AVMEDIA_TYPE_AUDIO;
    encoder->sample_fmt = AV_SAMPLE_FMT_FLTP;
    encoder->bit_rate = 256000;
    encoder->sample_rate = 48000;
    encoder->channel_layout = AV_CH_LAYOUT_STEREO; //AV_CH_LAYOUT_MONO;
    encoder->channels = av_get_channel_layout_nb_channels(encoder->channel_layout);
    encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    st = avformat_new_stream(fmt_ctx, codec);
    avcodec_parameters_from_context(st->codecpar, encoder);
    avcodec_open2(encoder, codec, nullptr);
}

void AudioRecorder::InitializeFrame() {
    frame = av_frame_alloc();
    if (frame != nullptr) {
        frame->pts = 0;
        frame->nb_samples = encoder->frame_size;
        frame->format = encoder->sample_fmt;
        frame->channel_layout = encoder->channel_layout;
        frame->sample_rate = encoder->sample_rate;
    }
    av_frame_get_buffer(frame, 0);
    av_frame_make_writable(frame);
}

void AudioRecorder::InitializeFifoBuffer() {
    fifo_buffer = av_audio_fifo_alloc(encoder->sample_fmt, encoder->channels, 1);
    if (fifo_buffer == nullptr) {
        printf("Init fifo fail.\n");
    }
}

void AudioRecorder::Encode() {
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    int ret = avcodec_send_frame(encoder, frame);
    if (ret < 0 || ret == AVERROR_EOF) {
        printf("Could not send packet for encoding\n");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(encoder, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            printf("Error encoding frame\n");
            break;
        }

        pkt.stream_index = st->index;
        if ((ret = av_interleaved_write_frame(fmt_ctx, &pkt)) < 0) {
                    printf("Error muxing packet!");
        }
        av_packet_unref(&pkt);
    }
}

int AudioRecorder::PushAudioBuffer(float *buffer, int total_samples, int channels) {
    uint8_t **converted_input_samples = nullptr;
    int samples_per_channel = total_samples/channels;
    av_samples_alloc_array_and_samples(&converted_input_samples, nullptr, 
        encoder->channels, samples_per_channel, encoder->sample_fmt, 0);

    for (int i = 0; i < total_samples; i++) {
        if (i % channels >= encoder->channels) {
            continue;
        }
        reinterpret_cast<float*>(converted_input_samples[i % channels])[i / channels] = buffer[i];
    }

    return av_audio_fifo_write(fifo_buffer, (void**)converted_input_samples, samples_per_channel);
}

void AudioRecorder::WriteFile() {
    if (av_audio_fifo_size(fifo_buffer) < encoder->frame_size) {
        return;
    }

    if(av_audio_fifo_read(fifo_buffer, (void**)&frame->data, encoder->frame_size) < 0) {
        printf("Read fifo fail");
    }

    frame->pts = av_rescale_q(++frame_count * frame->nb_samples, encoder->time_base, st->time_base);

    Encode();
}

void AudioRecorder::FinishFile() {
    frame_count = 0;

    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
        avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
        std::cout << "[AudioRecorder]: finished" << std::endl;
    }
}

AudioRecorder::~AudioRecorder() {
    FinishFile();
}
