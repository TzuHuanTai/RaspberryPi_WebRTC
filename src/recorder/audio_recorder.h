#ifndef AUDIO_RECODER_H_
#define AUDIO_RECODER_H_
#include "capture/pa_capture.h"
#include "recorder/recorder.h"

#include <string>
#include <future>
#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
}

class AudioRecorder : public Recorder<PaBuffer> {
public:
    static std::unique_ptr<AudioRecorder> CreateRecorder(
        std::shared_ptr<PaCapture> capture);
    AudioRecorder(std::shared_ptr<PaCapture> capture);
    ~AudioRecorder() {};
    void OnBuffer(PaBuffer buffer) override;

private:
    std::string encoder_name;
    AVAudioFifo* fifo_buffer;
    AVFrame *frame;

    void Encode(int stream_index);
    void InitializeFrame(AVCodecContext *encoder);
    void InitializeFifoBuffer(AVCodecContext *encoder);
    void CleanBuffer() override;
    void WriteFile() override;
    AVCodecContext *InitializeEncoder() override;
};

#endif
