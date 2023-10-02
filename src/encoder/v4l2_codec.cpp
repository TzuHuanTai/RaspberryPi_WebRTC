#include "encoder/v4l2_codec.h"

#include <future>

V4l2Codec::~V4l2Codec() {
    if (fd_ > 0) {
        ReleaseCodec();
    }
}

bool V4l2Codec::Open(const char *file_name) {
    fd_ = V4l2Util::OpenDevice(file_name);
    if (fd_ < 0) {
        return false;
    }
    return true;
}

bool V4l2Codec::PrepareBuffer(BufferGroup *gbuffer, int width, int height,
                              uint32_t pix_fmt, v4l2_buf_type type,
                              v4l2_memory memory, int buffer_num) {
    if (!V4l2Util::InitBuffer(fd_, gbuffer, type, memory)) {
        return false;
    }

    if (!V4l2Util::SetFormat(fd_, gbuffer, width, height, pix_fmt)) {
        return false;
    }

    if (!V4l2Util::AllocateBuffer(fd_, gbuffer, buffer_num)) {
        return false;
    }

    if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
        for (int i = 0; i < buffer_num; i++) {
            output_buffer_index_.push(i);
        }
    } else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        if (!V4l2Util::QueueBuffers(fd_, gbuffer)) {
            return false;
        }
    }

    return true;
}

void V4l2Codec::ResetProcessor() {
    processor_.reset(new Processor([&]() { CapturingFunction();}));
    processor_->Run();
}

void V4l2Codec::EmplaceBuffer(const uint8_t *byte, uint32_t length, 
                              std::function<void(Buffer)>on_capture) {
    auto output_result = std::async(std::launch::async, 
                                    &V4l2Codec::OutputBuffer,
                                    this, byte, length);

    // skip sending task if output_result is false
    if(output_result.get()) {
        capturing_tasks_.push(on_capture);
    }
}

bool V4l2Codec::OutputBuffer(const uint8_t *byte, uint32_t length) {
    if (output_buffer_index_.empty()) {
        return false;
    }

    int index = output_buffer_index_.front();
    output_buffer_index_.pop();

    memcpy((uint8_t *)output_.buffers[index].start, byte, length);
    
    if (!V4l2Util::QueueBuffer(fd_, &output_.buffers[index].inner)) {
        printf("error: QueueBuffer V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE. fd(%d) at index %d\n",
               fd_, index);
        return false;
    }
    return true;
}

bool V4l2Codec::CaptureBuffer(Buffer &buffer) {
    fd_set fds[2];
    fd_set *rd_fds = &fds[0]; /* for capture */
    fd_set *ex_fds = &fds[1]; /* for handle event */
    FD_ZERO(rd_fds);
    FD_SET(fd_, rd_fds);
    FD_ZERO(ex_fds);
    FD_SET(fd_, ex_fds);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int r = select(fd_ + 1, rd_fds, NULL, ex_fds, &tv);

    if (r <= 0) { // timeout or failed
        return false;
    }

    if (rd_fds && FD_ISSET(fd_, rd_fds)) {
        struct v4l2_buffer buf = {0};
        struct v4l2_plane out_planes = {0};
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        buf.m.planes = &out_planes;

        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (!V4l2Util::DequeueBuffer(fd_, &buf)) {
            return false;
        }
        output_buffer_index_.push(buf.index);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (!V4l2Util::DequeueBuffer(fd_, &buf)) {
            return false;
        }

        buffer.start = capture_.buffers[buf.index].start;
        buffer.length = buf.m.planes[0].bytesused;
        buffer.flags = buf.flags;

        if (!V4l2Util::QueueBuffer(fd_, &capture_.buffers[buf.index].inner)) {
            return false;
        }
    }

    if (ex_fds && FD_ISSET(fd_, ex_fds)) {
        fprintf(stderr, "Exception in fd(%d).\n", fd_);
        HandleEvent();
    }
    return true;
}

void V4l2Codec::CapturingFunction() {
    Buffer encoded_buffer = {};
    if(CaptureBuffer(encoded_buffer) && !capturing_tasks_.empty()) {
        auto task = capturing_tasks_.front();
        capturing_tasks_.pop();
        task(encoded_buffer);
    }
}

void V4l2Codec::ReleaseCodec() {
    output_buffer_index_ = {};
    processor_.reset();

    V4l2Util::StreamOff(fd_, output_.type);
    V4l2Util::StreamOff(fd_, capture_.type);

    V4l2Util::DeallocateBuffer(fd_, &output_);
    V4l2Util::DeallocateBuffer(fd_, &capture_);

    V4l2Util::CloseDevice(fd_);
    fd_ = -1;
}
