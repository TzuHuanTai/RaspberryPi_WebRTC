#ifndef V4L2_CODEC_
#define V4L2_CODEC_

#include "common/v4l2_utils.h"
#include "common/worker.h"

#include <functional>
#include <queue>

class V4l2Codec {
public:
    V4l2Codec(): fd_(0) {};
    virtual ~V4l2Codec();
    bool Open(const char *file_name);
    bool PrepareBuffer(BufferGroup *gbuffer, int width, int height,
                       uint32_t pix_fmt, v4l2_buf_type type,
                       v4l2_memory memory, int buffer_num, bool has_dmafd = false);
    void ResetWorker();
    bool EmplaceBuffer(Buffer &buffer, std::function<void(Buffer)>on_capture);
    void ReleaseCodec();

protected:
    int fd_;
    BufferGroup output_;
    BufferGroup capture_;
    std::queue<int> output_buffer_index_;
    std::unique_ptr<Worker> worker_;
    std::queue<std::function<void(Buffer)>> capturing_tasks_;
    virtual void HandleEvent() {};

private:
    bool OutputBuffer(Buffer &buffer);
    bool CaptureBuffer(Buffer &buffer);
    void CapturingFunction();
};

#endif // V4L2_CODEC_
