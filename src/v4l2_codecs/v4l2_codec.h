#ifndef V4L2_CODEC_
#define V4L2_CODEC_

#include "common/v4l2_utils.h"
#include "common/worker.h"

#include <atomic>
#include <functional>
#include <queue>

class V4l2Codec {
public:
    V4l2Codec(): fd_(0) {};
    virtual ~V4l2Codec();
    bool Open(const char *file_name);
    bool PrepareBuffer(V4l2BufferGroup *gbuffer, int width, int height,
                       uint32_t pix_fmt, v4l2_buf_type type,
                       v4l2_memory memory, int buffer_num, bool has_dmafd = false);
    void Start();
    void Stop();
    bool IsCapturing();
    void EmplaceBuffer(V4l2Buffer &buffer, std::function<void(V4l2Buffer)>on_capture);
    void ReleaseCodec();

protected:
    int fd_;
    std::atomic<bool> is_capturing;
    V4l2BufferGroup output_;
    V4l2BufferGroup capture_;
    std::queue<int> output_buffer_index_;
    std::unique_ptr<Worker> worker_;
    std::queue<std::function<void(V4l2Buffer)>> capturing_tasks_;
    virtual void HandleEvent() {};

private:
    const char *file_name_;
    bool OutputBuffer(V4l2Buffer &buffer);
    bool CaptureBuffer(V4l2Buffer &buffer);
    void CapturingFunction();
};

#endif // V4L2_CODEC_
