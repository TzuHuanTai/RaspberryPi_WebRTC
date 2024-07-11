#ifndef AUDIO_RECODER_H_
#define AUDIO_RECODER_H_

#include "capture/pa_capture.h"
#include "recorder/recorder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
}

class AudioRecorder : public Recorder<PaBuffer> {
public:
    static std::unique_ptr<AudioRecorder> Create(Args config);
    AudioRecorder(Args config);
    ~AudioRecorder();
    void OnBuffer(PaBuffer &buffer) override;
    void PreStart() override;

private:
    int sample_rate;;
    int channels = 2;
    int frame_size;
    unsigned int frame_count;
    std::string encoder_name;
    AVAudioFifo* fifo_buffer;
    AVSampleFormat sample_fmt = AV_SAMPLE_FMT_FLTP;
    AVFrame *frame;

    void Encode();
    void InitializeFrame();
    void InitializeFifoBuffer();
    void InitializeEncoderCtx(AVCodecContext* &encoder) override;
    bool ConsumeBuffer() override;
};

#endif // AUDIO_RECODER_H_
