#include "raw_buffer.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

rtc::scoped_refptr<RawBuffer> RawBuffer::Create(int width, int height, int size)
{
    return rtc::make_ref_counted<RawBuffer>(width, height, size);
}

RawBuffer::RawBuffer(int width, int height, int size)
    : width_(width),
      height_(height),
      size_(size),
      data_(static_cast<uint8_t *>(webrtc::AlignedMalloc(
          size_, kBufferAlignment))) {}

RawBuffer::~RawBuffer() {}

void RawBuffer::InitializeData()
{
    memset(data_.get(), 0, size_);
}

webrtc::VideoFrameBuffer::Type RawBuffer::type() const
{
    return Type::kNative;
}

int RawBuffer::width() const
{
    return width_;
}

int RawBuffer::height() const
{
    return height_;
}

rtc::scoped_refptr<webrtc::I420BufferInterface> RawBuffer::ToI420()
{
    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(width_, height_);
    return buffer;
}

int RawBuffer::Size() const
{
    return size_;
}

unsigned int RawBuffer::GetFlags()
{
    return flags_;
}

void RawBuffer::SetFlags(unsigned int flags)
{
    flags_ = flags;
}

const uint8_t *RawBuffer::Data() const
{
    return data_.get();
}

uint8_t *RawBuffer::MutableData()
{
    return const_cast<uint8_t *>(Data());
}
