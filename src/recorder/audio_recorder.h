#ifndef AUDIO_RECODER_H_
#define AUDIO_RECODER_H_
#include "capture/pa_capture.h"
#include "recorder/recorder.h"

#include <string>
#include <future>
#include <memory>
#include <mutex>

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
    void OnBuffer(PaBuffer buffer) override;
    void Initialize() override;

private:
    std::mutex codec_mux;
    AVSampleFormat sample_fmt = AV_SAMPLE_FMT_FLTP;
    int channels = 2;
    Args config;
    std::string encoder_name;
    AVAudioFifo* fifo_buffer;
    AVFrame *frame;

    void CloseCodec();
    void Encode(int stream_index);
    void InitializeFrame(AVCodecContext *encoder);
    void InitializeFifoBuffer(AVCodecContext *encoder);
    void InitializeEncoder() override;
};

#endif
