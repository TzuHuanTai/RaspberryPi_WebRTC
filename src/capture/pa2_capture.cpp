#include "capture/pa2_capture.h"

#define CHANNELS 2

std::shared_ptr<Pa2Capture> Pa2Capture::Create(Args args) {
    auto ptr = std::make_shared<Pa2Capture>(args);
    ptr->CreateFloat32Source(args.sample_rate);
    ptr->StartCapture();
    return ptr;
}

Pa2Capture::Pa2Capture(Args args) 
    : PaCapture(args),
      config_(args) {}

Pa2Capture::~Pa2Capture() {
    worker_.reset();
    pa_stream_disconnect(stream);
    pa_stream_unref(stream);
    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(m);
}

Args Pa2Capture::config() const {
    return config_;
}

void Pa2Capture::CreateFloat32Source(int sample_rate) {
    m = pa_mainloop_new();
    mainloop_api = pa_mainloop_get_api(m);
    ctx = pa_context_new(mainloop_api, "Microphone");

    pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    do {
        pa_mainloop_iterate(m, 1, nullptr);
        ctx_state = pa_context_get_state(ctx);
    } while (ctx_state != PA_CONTEXT_READY && ctx_state != PA_CONTEXT_FAILED);

    if (ctx_state == PA_CONTEXT_FAILED) {
        std::cerr << "Failed to connect to PulseAudio server" << std::endl;
        return;
    }

    int error;
    pa_sample_spec spec = {};
    spec.format = PA_SAMPLE_FLOAT32LE;
    spec.channels = CHANNELS;
    spec.rate = sample_rate;

    stream = pa_stream_new(ctx, "Recording Stream", &spec, nullptr);
    pa_stream_set_read_callback(stream, &ReadCallback, this);
    pa_stream_set_state_callback(stream, &StateCallback, this);

    pa_stream_connect_record(stream, nullptr, nullptr, PA_STREAM_NOFLAGS);
}

void Pa2Capture::ReadCallback(pa_stream *s, size_t length, void *user_data) {
    Pa2Capture* capture = reinterpret_cast<Pa2Capture*>(user_data);
    capture->CaptureSamples();
}

void Pa2Capture::StateCallback(pa_stream *s, void *user_data) {
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
            std::cout << "Stream is ready." << std::endl;
            break;
        case PA_STREAM_FAILED:
            std::cerr << "Stream failed: " << pa_strerror(pa_context_errno(pa_stream_get_context(s))) << std::endl;
            break;
        case PA_STREAM_TERMINATED:
            std::cerr << "Stream terminated." << std::endl;
            break;
        default:
            break;
    }
}

void Pa2Capture::CaptureSamples() {
    const void* data = nullptr;
    size_t length = 0;

    if (pa_stream_peek(stream, &data, &length) < 0) {
        std::cerr << "Failed to peek into stream" << std::endl;
        return;
    }

    uint8_t copy_buf[length];
    memcpy(copy_buf, (const uint8_t*)data, length);

    shared_buffer_ = {.start = copy_buf,
                      .length = static_cast<unsigned int>(length / sizeof(float)),
                      .channels = CHANNELS };
    Next(shared_buffer_);

    pa_stream_drop(stream);
}

void Pa2Capture::StartCapture() {
    worker_.reset(new Worker("PaCapture2", [&]() {
        pa_mainloop_iterate(m, 1, nullptr);
    }));
    worker_->Run();
}
