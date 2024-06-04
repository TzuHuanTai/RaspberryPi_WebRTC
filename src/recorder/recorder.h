#ifndef RECORDER_H_
#define RECORDER_H_
#include "common/interface/subject.h"
#include <atomic>
#include <string>
#include <future>
#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

template<typename T>
class Recorder {
public:
    typedef std::function<void(AVPacket *pkt)> OnPacketedFunc;

    Recorder() : frame_count(0),
                 is_started(false) {};
    ~Recorder() { };

    virtual void OnBuffer(T &buffer) = 0;

    virtual void Initialize() {
        InitializeEncoder();
    }

    virtual void ResetCodecs() {}

    bool AddStream(AVFormatContext *output_fmt_ctx) {
        frame_count = 0;
        InitializeEncoder();
        st = avformat_new_stream(output_fmt_ctx, encoder->codec);
        avcodec_parameters_from_context(st->codecpar, encoder);

        if (st == nullptr) {
            return false;
        }
        return true;
    }

    void OnPacketed(OnPacketedFunc fn) {
        on_packeted = fn;
    }

    virtual void Pause() = 0;

    virtual void Start() {
        is_started = true;
    }

protected:
    bool is_started;
    unsigned int frame_count;
    OnPacketedFunc on_packeted;

    AVCodecContext *encoder;
    AVStream *st;

    virtual void InitializeEncoder() = 0;
    
    void OnPacketed(AVPacket *pkt) {
        if (on_packeted){
            on_packeted(pkt);
        }
    }
};

#endif
