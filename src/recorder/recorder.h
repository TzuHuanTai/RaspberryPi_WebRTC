#ifndef RECORDER_H_
#define RECORDER_H_
#include "common/interface/subject.h"
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

    Recorder(std::shared_ptr<Subject<T>> capture)
        : is_recording(false),
          frame_count(0),
          capture(capture) { };
    ~Recorder() {
        observer->UnSubscribe();
        Stop();
    };

    void Initialize() {
        encoder = InitializeEncoder();
        SubscribeBufferSource(capture);
    }

    bool AddStream(AVFormatContext *output_fmt_ctx) {
        fmt_ctx = output_fmt_ctx;
        st = avformat_new_stream(output_fmt_ctx, encoder->codec);
        avcodec_parameters_from_context(st->codecpar, encoder);

        if (st == nullptr) {
            return false;
        }
        return true;
    }

    void Start() {
        if (fmt_ctx == nullptr || st == nullptr) {
            printf("[Recorder]: context or stream was not created.");
            return;
        }
        is_recording = true;
        // CleanBuffer();
        consumer = std::async(std::launch::async, 
                              &Recorder::AsyncWriteBackground, this);
    }

    std::future<bool> Stop() {
        return std::async(std::launch::async,[&]() -> bool {
            is_recording = false;
            if (consumer.valid()) {
                consumer.wait();
            }
            frame_count = 0;
            return true;
        });
    }

    void OnEncoded(OnPacketedFunc fn) {
        on_packeted = fn;
    }

    void OnEncoded(AVPacket *pkt) {
        if (on_packeted){
            on_packeted(pkt);
        }
    }

protected:
    bool is_recording;
    unsigned int frame_count;
    std::future<void> consumer;
    std::shared_ptr<Subject<T>> capture;
    OnPacketedFunc on_packeted;

    AVCodecContext *encoder;
    AVFormatContext *fmt_ctx;
    AVStream *st;

    virtual AVCodecContext *InitializeEncoder() = 0;
    virtual void CleanBuffer() = 0;
    virtual void WriteFile() = 0;
    virtual void OnBuffer(T buffer) = 0;
    void AsyncWriteBackground() {
        while (is_recording) {
            WriteFile();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    std::shared_ptr<Observable<T>> observer;

    void SubscribeBufferSource(std::shared_ptr<Subject<T>> capture) {
        observer = capture->AsObservable();
        observer->Subscribe([&](T buffer) {
            if (is_recording) {
                OnBuffer(buffer);
            }
        });
    }
};

#endif
