#include "recorder/audio_recorder.h"

std::unique_ptr<AudioRecorder> AudioRecorder::Create(Args config) {
    auto ptr = std::make_unique<AudioRecorder>(config);
    ptr->Initialize();
    return ptr;
}

AudioRecorder::AudioRecorder(Args config)
    : Recorder(),
      config(config),
      encoder_name("aac") { }

AudioRecorder::~AudioRecorder() {}

void AudioRecorder::Initialize() {
    InitializeEncoder();
    InitializeFrame(encoder);
    InitializeFifoBuffer(encoder);
}

void AudioRecorder::CloseCodec() {
    if (encoder) {
        avcodec_close(encoder);
        avcodec_free_context(&encoder);
    }
}

void AudioRecorder::InitializeEncoder() {
    std::lock_guard<std::mutex> lock(codec_mux);
    CloseCodec();
    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    encoder = avcodec_alloc_context3(codec);
    encoder->codec_type = AVMEDIA_TYPE_AUDIO;
    encoder->sample_fmt = sample_fmt;
    encoder->bit_rate = 128000;
    encoder->sample_rate = config.sample_rate;
    encoder->channel_layout = AV_CH_LAYOUT_STEREO;

    channels = av_get_channel_layout_nb_channels(encoder->channel_layout);
    encoder->channels = channels;
    encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    avcodec_open2(encoder, codec, nullptr);
}

void AudioRecorder::InitializeFrame(AVCodecContext *encoder) {
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

void AudioRecorder::InitializeFifoBuffer(AVCodecContext *encoder) {
    fifo_buffer = av_audio_fifo_alloc(encoder->sample_fmt, encoder->channels, 1);
    if (fifo_buffer == nullptr) {
        printf("Init fifo fail.\n");
    }
}

void AudioRecorder::Encode(int stream_index) {
    std::lock_guard<std::mutex> lock(codec_mux);
    AVPacket pkt;
    av_init_packet(&pkt);

    if(av_audio_fifo_read(fifo_buffer, (void**)&frame->data, encoder->frame_size) < 0) {
        printf("Read fifo fail");
    }

    frame->pts = av_rescale_q(frame_count++ * frame->nb_samples, 
                              encoder->time_base, st->time_base);

    int ret = avcodec_send_frame(encoder, frame);
    if (ret < 0 || ret == AVERROR_EOF) {
        printf("Could not send packet for encoding\n");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(encoder, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret < 0) {
            printf("Error encoding frame\n");
            break;
        }

        pkt.stream_index = stream_index;
        OnPacketed(&pkt);
    }
    av_packet_unref(&pkt);
}

void AudioRecorder::OnBuffer(PaBuffer buffer) {
    uint8_t **converted_input_samples = nullptr;
    int samples_per_channel = buffer.length / buffer.channels;

    if (av_samples_alloc_array_and_samples(&converted_input_samples, nullptr, 
                                           channels, samples_per_channel, sample_fmt, 0) < 0) {
        // Handle allocation error
        return;
    }

    auto data = reinterpret_cast<float*>(buffer.start);
    for (int i = 0; i < buffer.length; ++i) {
        int channel_index = i % buffer.channels;
        if (channel_index < channels) {
            reinterpret_cast<float*>(converted_input_samples[channel_index])
                [i / buffer.channels] = data[i];
        }
    }

    if (av_audio_fifo_write(fifo_buffer, reinterpret_cast<void**>(converted_input_samples),
        samples_per_channel) < samples_per_channel) {
        // Handle write error
    }

    while (is_started && encoder && st && 
           av_audio_fifo_size(fifo_buffer) >= encoder->frame_size) {
        Encode(st->index);
    }

    if (converted_input_samples) {
        av_freep(&converted_input_samples[0]);
        av_freep(&converted_input_samples);
    }
}
