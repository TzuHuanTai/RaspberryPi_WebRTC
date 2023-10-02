#ifndef V4L2_CODEC_
#define V4L2_CODEC_

#include "v4l2_utils.h"
#include "processor.h"

#include <functional>
#include <queue>

class V4l2Codec {
public:
    V4l2Codec(): fd_(-1) {};
    virtual ~V4l2Codec();
    bool Open(const char *file_name);
    bool PrepareBuffer(BufferGroup *gbuffer, int width, int height,
                       uint32_t pix_fmt, v4l2_buf_type type,
                       v4l2_memory memory, int buffer_num);
    void ResetProcessor();
    void EmplaceBuffer(const uint8_t *byte, uint32_t length, 
                       std::function<void(Buffer)>on_capture);
    void ReleaseCodec();

protected:
    int fd_;
    BufferGroup output_;
    BufferGroup capture_;
    std::queue<int> output_buffer_index_;
    std::unique_ptr<Processor> processor_;
    std::queue<std::function<void(Buffer)>> capturing_tasks_;
    virtual void HandleEvent() {};

private:
    bool OutputBuffer(const uint8_t *byte, uint32_t length);
    bool CaptureBuffer(Buffer &buffer);
    void CapturingFunction();
};

#endif // V4L2_CODEC_
