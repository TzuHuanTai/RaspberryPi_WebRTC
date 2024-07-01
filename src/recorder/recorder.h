#ifndef RECORDER_H_
#define RECORDER_H_
#include "common/interface/subject.h"
#include "common/worker.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

template<typename T>
class Recorder {
public:
    using OnPacketedFunc = std::function<void(AVPacket *pkt)>;

    Recorder() : is_started(false) {};
    ~Recorder() { };

    virtual void OnBuffer(T &buffer) = 0;

    void Initialize() {
        encoder = InitializeEncoderCtx();
        worker.reset(new Worker("Recorder", [this]() { 
            while (is_started && ConsumeBuffer()) {}
            usleep(15000);
        }));
        worker->Run();
    }

    void ResetCodecCtx() {
        encoder = InitializeEncoderCtx();
    }

    virtual void PostStop() {};
    virtual void PreStart() {};

    bool AddStream(AVFormatContext *output_fmt_ctx) {
        st = avformat_new_stream(output_fmt_ctx, encoder->codec);
        avcodec_parameters_from_context(st->codecpar, encoder);

        return st != nullptr;
    }

    void OnPacketed(OnPacketedFunc fn) {
        on_packeted = fn;
    }

    void Stop() {
        is_started = false;
        PostStop();
        ResetCodecCtx();
    }

    void Start() {
        PreStart();
        is_started = true;
    }

protected:
    bool is_started;
    OnPacketedFunc on_packeted;
    std::unique_ptr<Worker> worker;
    AVCodecContext *encoder;
    AVStream *st;

    virtual AVCodecContext* InitializeEncoderCtx() = 0;
    virtual bool ConsumeBuffer() = 0;
    void OnPacketed(AVPacket *pkt) {
        if (on_packeted){
            on_packeted(pkt);
        }
    }
};

#endif  // RECORDER_H_
