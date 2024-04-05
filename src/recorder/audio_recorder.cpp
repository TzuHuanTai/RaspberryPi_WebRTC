#include "recorder/audio_recorder.h"

std::unique_ptr<AudioRecorder> AudioRecorder::CreateRecorder(
    std::shared_ptr<PaCapture> capture) {
    auto ptr = std::make_unique<AudioRecorder>(capture);
    ptr->Initialize();
    return ptr;
}

AudioRecorder::AudioRecorder(std::shared_ptr<PaCapture> capture)
    : Recorder(capture),
      encoder_name("aac") { }

AVCodecContext *AudioRecorder::InitializeEncoder() {
    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    auto encoder = avcodec_alloc_context3(codec);
    encoder->codec_type = AVMEDIA_TYPE_AUDIO;
    encoder->sample_fmt = AV_SAMPLE_FMT_FLTP;
    encoder->bit_rate = 128000;
    encoder->sample_rate = 48000;
    encoder->channel_layout = AV_CH_LAYOUT_STEREO;
    encoder->channels = av_get_channel_layout_nb_channels(encoder->channel_layout);
    encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    avcodec_open2(encoder, codec, nullptr);

    InitializeFrame(encoder);
    InitializeFifoBuffer(encoder);

    return encoder;
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
    AVPacket pkt;
    av_init_packet(&pkt);

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
        OnEncoded(&pkt);
    }
    av_packet_unref(&pkt);
}

void AudioRecorder::OnBuffer(PaBuffer buffer) {
    uint8_t **converted_input_samples = nullptr;
    int samples_per_channel = buffer.length / buffer.channels;
    av_samples_alloc_array_and_samples(&converted_input_samples, nullptr, 
        encoder->channels, samples_per_channel, encoder->sample_fmt, 0);

    auto data = (float*)buffer.start;
    for (int i = 0; i < buffer.length; i++) {
        if (i % buffer.channels >= encoder->channels) {
            continue;
        }
        reinterpret_cast<float*>(converted_input_samples[i % buffer.channels])
            [i / buffer.channels] = data[i];
    }

    av_audio_fifo_write(fifo_buffer, (void**)converted_input_samples, samples_per_channel);
}

void AudioRecorder::WriteFile() {
    while (av_audio_fifo_size(fifo_buffer) >= encoder->frame_size) {
        if(av_audio_fifo_read(fifo_buffer, (void**)&frame->data, encoder->frame_size) < 0) {
            printf("Read fifo fail");
        }

        frame->pts = av_rescale_q(frame_count++ * frame->nb_samples, encoder->time_base, st->time_base);

        Encode(st->index);
    }
}

void AudioRecorder::CleanBuffer() {
    if (av_audio_fifo_size(fifo_buffer) > 0) {
        av_audio_fifo_drain(fifo_buffer, av_audio_fifo_size(fifo_buffer));
    }
}
