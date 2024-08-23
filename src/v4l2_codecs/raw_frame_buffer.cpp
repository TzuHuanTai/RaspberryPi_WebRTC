#include "v4l2_codecs/raw_frame_buffer.h"


rtc::scoped_refptr<RawFrameBuffer> RawFrameBuffer::Create(int width, int height,
                                                V4l2Buffer buffer) {
    return rtc::make_ref_counted<RawFrameBuffer>(width, height, buffer);
}

RawFrameBuffer::RawFrameBuffer(int width, int height, V4l2Buffer buffer)
    : width_(width),
      height_(height),
      buffer_(buffer) {}

RawFrameBuffer::~RawFrameBuffer() {}

webrtc::VideoFrameBuffer::Type RawFrameBuffer::type() const { return Type::kNative; }

int RawFrameBuffer::width() const { return width_; }

int RawFrameBuffer::height() const { return height_; }

rtc::scoped_refptr<webrtc::I420BufferInterface> RawFrameBuffer::ToI420() {
    rtc::scoped_refptr<webrtc::I420Buffer> buffer = webrtc::I420Buffer::Create(width_, height_);

    memcpy(buffer->MutableDataY(), (uint8_t *)buffer_.start, buffer_.length);
    return buffer;
}

V4l2Buffer RawFrameBuffer::GetBuffer() { return buffer_; }
