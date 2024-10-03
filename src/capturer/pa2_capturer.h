#ifndef PA2_CAPTURER_H_
#define PA2_CAPTURER_H_

#include <cstring>
#include <iostream>

#include <pulse/pulseaudio.h>

#include "args.h"
#include "capturer/pa_capturer.h"
#include "common/interface/subject.h"
#include "common/worker.h"

class Pa2Capturer : public PaCapturer {
  public:
    static std::shared_ptr<Pa2Capturer> Create(Args args);
    static void ReadCallback(pa_stream *s, size_t length, void *userdata);
    static void StateCallback(pa_stream *s, void *user_data);
    Pa2Capturer(Args args);
    ~Pa2Capturer();
    Args config() const;
    void StartCapture();

  private:
    Args config_;

    pa_mainloop *m;
    pa_mainloop_api *mainloop_api;
    pa_context *ctx;
    pa_context_state_t ctx_state;
    pa_stream *stream;

    PaBuffer shared_buffer_;
    std::unique_ptr<Worker> worker_;

    void CaptureSamples();
    void CreateFloat32Source(int sample_rate);
};

#endif
