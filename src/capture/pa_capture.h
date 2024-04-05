#ifndef PA_CAPTURE_H_
#define PA_CAPTURE_H_

#include <pulse/simple.h>
#include <pulse/error.h>

#include "common/interface/subject.h"
#include "common/worker.h"

struct PaBuffer {
    uint8_t *start;
    unsigned int length;
    unsigned int channels;
};

class PaCapture : public Subject<PaBuffer> {
public:
    static std::shared_ptr<PaCapture> Create();
    PaCapture() {};
    ~PaCapture();
    void StartCapture();

private:
    pa_simple *src;
    PaBuffer shared_buffer_;
    std::unique_ptr<Worker> worker_;

    void CaptureSamples();
    void CreateFloat32Source();
};

#endif
