#ifndef PA_CAPTURE_H_
#define PA_CAPTURE_H_

#include "args.h"
#include "common/interface/subject.h"
#include "common/worker.h"

#include <pulse/error.h>
#include <pulse/simple.h>

struct PaBuffer {
    uint8_t *start;
    unsigned int length;
    unsigned int channels;
};

class PaCapture : public Subject<PaBuffer> {
public:
    static std::shared_ptr<PaCapture> Create(Args args);
    PaCapture(Args args);
    ~PaCapture();
    Args config() const;
    void StartCapture();

private:
    Args config_;
    pa_simple *src;
    PaBuffer shared_buffer_;
    std::unique_ptr<Worker> worker_;

    void CaptureSamples();
    void CreateFloat32Source(int sample_rate);
};

#endif
