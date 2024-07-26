#include "recorder/audio_recorder.h"
#include "common/logging.h"

std::unique_ptr<AudioRecorder> AudioRecorder::Create(Args config) {
    auto ptr = std::make_unique<AudioRecorder>(config);
    ptr->Initialize();
    ptr->InitializeFrame();
    ptr->InitializeFifoBuffer();
    return ptr;
}

AudioRecorder::AudioRecorder(Args config)
    : Recorder(),
      sample_rate(config.sample_rate),
      channels(2),
      sample_fmt(AV_SAMPLE_FMT_FLTP),
      encoder_name("aac") {}

AudioRecorder::~AudioRecorder() {}

void AudioRecorder::InitializeEncoderCtx(AVCodecContext *&encoder) {
    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    encoder = avcodec_alloc_context3(codec);
    encoder->codec_type = AVMEDIA_TYPE_AUDIO;
    encoder->sample_fmt = sample_fmt;
    encoder->bit_rate = 128000;
    encoder->sample_rate = sample_rate;
    encoder->channel_layout = AV_CH_LAYOUT_STEREO;

    channels = av_get_channel_layout_nb_channels(encoder->channel_layout);
    encoder->channels = channels;
    encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    avcodec_open2(encoder, codec, nullptr);
}

void AudioRecorder::InitializeFrame() {
    frame = av_frame_alloc();
    frame_size = encoder->frame_size;
    if (frame != nullptr) {
        frame->pts = 0;
        frame->nb_samples = frame_size;
        frame->format = encoder->sample_fmt;
        frame->channel_layout = encoder->channel_layout;
        frame->sample_rate = encoder->sample_rate;
    }
    av_frame_get_buffer(frame, 0);
    av_frame_make_writable(frame);
}

void AudioRecorder::InitializeFifoBuffer() {
    fifo_buffer = av_audio_fifo_alloc(sample_fmt, channels, 1);
    if (fifo_buffer == nullptr) {
        DEBUG_PRINT("Failed to initialize audio fifo buffer.");
    }
}

void AudioRecorder::Encode() {
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;

    if (av_audio_fifo_read(fifo_buffer, (void **)&frame->data, frame_size) < 0) {
        DEBUG_PRINT("Failed to read audio fifo buffer.");
    }

    frame->pts = frame_count * frame->nb_samples;
    frame_count++;

    int ret = avcodec_send_frame(encoder, frame);
    if (ret < 0 || ret == AVERROR_EOF) {
        DEBUG_PRINT("Could not send packet to encoder");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(encoder, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret < 0) {
            DEBUG_PRINT("Failed to encode the frame");
            break;
        }

        pkt.stream_index = st->index;
        pkt.pts = av_rescale_q(pkt.pts, encoder->time_base, st->time_base);
        pkt.dts = av_rescale_q(pkt.dts, encoder->time_base, st->time_base);
        pkt.duration = av_rescale_q(pkt.duration, encoder->time_base, st->time_base);

        OnPacketed(&pkt);

        av_packet_unref(&pkt);
    }
}

void AudioRecorder::OnBuffer(PaBuffer &buffer) {
    uint8_t **converted_input_samples = nullptr;
    int samples_per_channel = buffer.length / buffer.channels;

    if (av_samples_alloc_array_and_samples(&converted_input_samples, nullptr, channels,
                                           samples_per_channel, sample_fmt, 0) < 0) {
        // Handle allocation error
        return;
    }

    auto data = reinterpret_cast<float *>(buffer.start);
    for (int i = 0; i < buffer.length; ++i) {
        int channel_index = i % buffer.channels;
        if (channel_index < channels) {
            reinterpret_cast<float *>(converted_input_samples[channel_index])[i / buffer.channels] =
                data[i];
        }
    }

    if (av_audio_fifo_write(fifo_buffer, reinterpret_cast<void **>(converted_input_samples),
                            samples_per_channel) < samples_per_channel) {
        // Handle write error
    }

    if (converted_input_samples) {
        av_freep(&converted_input_samples[0]);
        av_freep(&converted_input_samples);
        converted_input_samples = nullptr;
    }
}

bool AudioRecorder::ConsumeBuffer() {
    if (av_audio_fifo_size(fifo_buffer) < frame_size) {
        return false;
    }
    Encode();
    return true;
}

void AudioRecorder::PreStart() {
    frame_count = 0;

    // Skip redundant frame to ensure sync and keep 1 frame only
    int fifo_size = av_audio_fifo_size(fifo_buffer);
    if (fifo_size > frame_size) {
        av_audio_fifo_drain(fifo_buffer, fifo_size - frame_size);
    }
}
