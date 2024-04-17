#ifndef PA2_CAPTURE_H_
#define PA2_CAPTURE_H_

#include "args.h"
#include "capture/pa_capture.h"
#include "common/interface/subject.h"
#include "common/worker.h"

#include <pulse/pulseaudio.h>
#include <iostream>
#include <cstring>

class Pa2Capture : public PaCapture {
public:
    static std::shared_ptr<Pa2Capture> Create(Args args);
    static void ReadCallback(pa_stream *s, size_t length, void *userdata);
    static void StateCallback(pa_stream *s, void *user_data);
    Pa2Capture(Args args);
    ~Pa2Capture();
    Args config() const;
    void StartCapture();

private:
    Args config_;

    pa_mainloop* m;
    pa_mainloop_api* mainloop_api;
    pa_context* ctx;
    pa_context_state_t ctx_state;
    pa_stream* stream;

    PaBuffer shared_buffer_;
    std::unique_ptr<Worker> worker_;

    void CaptureSamples();
    void CreateFloat32Source(int sample_rate);
};

#endif
