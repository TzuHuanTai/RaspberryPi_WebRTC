#ifndef RAW_FRAME_BUFFER_H_
#define RAW_FRAME_BUFFER_H_

#include <api/video/i420_buffer.h>
#include <api/video/video_frame.h>
#include <common_video/include/video_frame_buffer.h>
#include <rtc_base/memory/aligned_malloc.h>

#include "common/v4l2_utils.h"

class RawFrameBuffer : public webrtc::VideoFrameBuffer {
  public:
    static rtc::scoped_refptr<RawFrameBuffer> Create(int width, int height, V4l2Buffer buffer);

    Type type() const override;
    int width() const override;
    int height() const override;
    rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;
    V4l2Buffer GetBuffer();

  protected:
    RawFrameBuffer(int width, int height, V4l2Buffer buffer);
    ~RawFrameBuffer() override;

  private:
    const int width_;
    const int height_;
    unsigned int flags_;
    V4l2Buffer buffer_;
    const std::unique_ptr<uint8_t, webrtc::AlignedFreeDeleter> data_;
};

#endif // RAW_BUFFER_H_
