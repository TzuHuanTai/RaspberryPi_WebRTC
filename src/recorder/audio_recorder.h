#ifndef AUDIO_RECODER_H_
#define AUDIO_RECODER_H_

#include <pulse/simple.h>
#include <pulse/error.h>

#include <string>
#include <future>
#include <queue>
#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/audio_fifo.h>
}

class AudioRecorder {
public:
    static std::unique_ptr<AudioRecorder> CreateRecorder(std::string record_path);
    AudioRecorder(std::string record_path);
    ~AudioRecorder();
    int PushAudioBuffer(float *buffer, int total_samples, int channels);
    void WriteFile();

protected:
    double t = 0;
    std::string record_path;
    unsigned int frame_count;
    std::string encoder_name;

    AVCodecContext *encoder;
    AVFormatContext *fmt_ctx;
    AVStream *st;
    AVAudioFifo* fifo_buffer;
    AVFrame *frame;
    AVPacket pkt;

    std::string PrefixZero(int stc, int digits);
    std::string GenerateFilename();

    void Encode();
    bool InitializeContainer();
    void InitializeFifoBuffer();
    void InitializeFrame();
    void AddAudioStream();
    void FinishFile();
};

#endif
