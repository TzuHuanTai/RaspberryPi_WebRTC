#ifndef RECORDER_H_
#define RECORDER_H_
#include "common/interface/subject.h"
#include "common/worker.h"

#include <mutex>

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
    ~Recorder() {
        avcodec_free_context(&encoder);
    };

    virtual void OnBuffer(T &buffer) = 0;

    void Initialize() {
        avcodec_free_context(&encoder);
        InitializeEncoderCtx(encoder);
        worker.reset(new Worker("Recorder", [this]() { 
            while (is_started && ConsumeBuffer()) {}
            usleep(100000);
        }));
    }

    virtual void PostStop() {};
    virtual void PreStart() {};

    bool AddStream(AVFormatContext *output_fmt_ctx) {
        file_url = output_fmt_ctx->url;
        st = avformat_new_stream(output_fmt_ctx, encoder->codec);
        avcodec_parameters_from_context(st->codecpar, encoder);

        return st != nullptr;
    }

    void OnPacketed(OnPacketedFunc fn) {
        on_packeted = fn;
    }

    void Stop() {
        is_started = false;
        worker.reset();
        PostStop();
    }

    void Start() {
        Initialize();
        PreStart();
        is_started = true;
        worker->Run();
    }

protected:
    std::atomic<bool> is_started;
    std::string file_url;
    OnPacketedFunc on_packeted;
    std::unique_ptr<Worker> worker;
    AVCodecContext *encoder;
    AVStream *st;

    virtual void InitializeEncoderCtx(AVCodecContext* &encoder) = 0;
    virtual bool ConsumeBuffer() = 0;
    void OnPacketed(AVPacket *pkt) {
        if (on_packeted){
            on_packeted(pkt);
        }
    }
};

#endif  // RECORDER_H_
