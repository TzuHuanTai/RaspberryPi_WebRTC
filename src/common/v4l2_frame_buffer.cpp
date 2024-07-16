#include "common/v4l2_frame_buffer.h"
#include "common/logging.h"

#include <third_party/libyuv/include/libyuv.h>

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

rtc::scoped_refptr<V4l2FrameBuffer> V4l2FrameBuffer::Create(
    int width, int height, int size, uint32_t format) {
    return rtc::make_ref_counted<V4l2FrameBuffer>(width, height, size, format);
}

V4l2FrameBuffer::V4l2FrameBuffer(int width, int height, int size, uint32_t format)
    : width_(width),
      height_(height),
      format_(format),
      size_(size),
      flags_(0),
      timestamp_({0, 0}),
      data_(static_cast<uint8_t *>(webrtc::AlignedMalloc(size_, kBufferAlignment))) {}

V4l2FrameBuffer::~V4l2FrameBuffer() {}

webrtc::VideoFrameBuffer::Type V4l2FrameBuffer::type() const {
    return Type::kNative;
}

int V4l2FrameBuffer::width() const {
    return width_;
}

int V4l2FrameBuffer::height() const {
    return height_;
}

uint32_t V4l2FrameBuffer::format() const {
    return format_;
}

unsigned int V4l2FrameBuffer::size() const {
    return size_;
}

unsigned int V4l2FrameBuffer::flags() const {
    return flags_;
}

timeval V4l2FrameBuffer::timestamp() const {
    return timestamp_;
}

rtc::scoped_refptr<webrtc::I420BufferInterface> V4l2FrameBuffer::ToI420() {
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer(
        webrtc::I420Buffer::Create(width_, height_));
    i420_buffer->InitializeData();

    if (format_ == V4L2_PIX_FMT_MJPEG) {
        if (libyuv::ConvertToI420((uint8_t *)data_.get(), size_,
                i420_buffer.get()->MutableDataY(), i420_buffer.get()->StrideY(),
                i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
                i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(),
                0, 0, width_, height_, width_, height_, libyuv::kRotate0,
                libyuv::FOURCC_MJPG) < 0) {
            ERROR_PRINT("Mjpeg ConvertToI420 Failed");
        }
    } else if (format_ == V4L2_PIX_FMT_YUV420) {
        memcpy(i420_buffer->MutableDataY(), data_.get(), size_);
    } else if (format_ == V4L2_PIX_FMT_H264) {
        // use hw decoded frame from track.
    }

    return i420_buffer;
}

void V4l2FrameBuffer::CopyBuffer(const uint8_t* data, int size,
    unsigned int flags, timeval timestamp) {
    size_ = size;
    flags_ = flags;
    timestamp_ = timestamp;
    memcpy(data_.get(), data, size);
}

const uint8_t *V4l2FrameBuffer::Data() const {
    return data_.get();
}

uint8_t *V4l2FrameBuffer::MutableData() {
    return const_cast<uint8_t*>(Data());
}
