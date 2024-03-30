// g++ -o pulseaudio-capture pulseaudio-capture.cpp -lpulse-simple -lpulse
#include <pulse/simple.h>
#include <pulse/error.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <thread>
#include <unistd.h>

#include "recorder/audio_recorder.h"

#define BUFSIZE 1024
#define CHANNELS 2
#define SAMPLE_RATE 48000

pa_simple* CreateAudioSourceObject() {
    pa_simple *s = nullptr;
    pa_sample_spec ss;
    int error;

    ss.format = PA_SAMPLE_FLOAT32LE;
    ss.channels = CHANNELS;
    ss.rate = SAMPLE_RATE;

    s = pa_simple_new(nullptr, "Microphone", PA_STREAM_RECORD, nullptr, "record", &ss, nullptr, nullptr, &error);
    if (!s) {
        std::cerr << "pa_simple_new() failed: " << pa_strerror(error) << std::endl;
        return nullptr;
    }
    return s;
}

void CaptureThread(pa_simple *src_obj, bool &abort) {
    int error;
    uint8_t buf[BUFSIZE * sizeof(float)];

    auto recorder = AudioRecorder::CreateRecorder("./");

    while (!abort) {
        if (pa_simple_read(src_obj, buf, sizeof(buf), &error) < 0) {
            std::cerr << "pa_simple_read() failed: " << pa_strerror(error) << std::endl;
            if (src_obj) {
                pa_simple_free(src_obj);
            }
            return;
        }
        recorder->PushAudioBuffer((float*)buf, sizeof(buf)/sizeof(float), CHANNELS);
        recorder->WriteFile();
    }
}

int main(int argc, char *argv[]) {
    bool abort = false;
    auto src_obj = CreateAudioSourceObject();
    std::thread capture_thread(CaptureThread, src_obj, std::ref(abort));
    
    sleep(15);
    abort = true;
    capture_thread.join();

    if (src_obj) {
        pa_simple_free(src_obj);
    }

    return 0;
}
