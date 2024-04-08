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

    Recorder() : frame_count(0) { };
    ~Recorder() { };

    virtual void OnBuffer(T buffer) = 0;

    virtual void Initialize() {
        InitializeEncoder();
    }

    bool AddStream(AVFormatContext *output_fmt_ctx) {
        frame_count = 0;
        InitializeEncoder();
        fmt_ctx = output_fmt_ctx;
        st = avformat_new_stream(output_fmt_ctx, encoder->codec);
        avcodec_parameters_from_context(st->codecpar, encoder);

        if (st == nullptr) {
            return false;
        }
        return true;
    }

    void OnEncoded(OnPacketedFunc fn) {
        on_packeted = fn;
    }

protected:
    unsigned int frame_count;
    OnPacketedFunc on_packeted;

    AVCodecContext *encoder;
    volatile AVFormatContext *fmt_ctx;
    AVStream *st;

    virtual void InitializeEncoder() = 0;
    
    void OnEncoded(AVPacket *pkt) {
        if (on_packeted){
            on_packeted(pkt);
        }
    }
};

#endif
