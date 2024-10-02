#ifndef LIBCAMERA_CAPTURE_H_
#define LIBCAMERA_CAPTURE_H_

#include <vector>

#include <libcamera/libcamera.h>
#include <modules/video_capture/video_capture.h>

#include "args.h"
#include "common/interface/subject.h"
#include "common/worker.h"
#include "common/v4l2_frame_buffer.h"
#include "common/v4l2_utils.h"

class LibcameraCapture {
  public:
    LibcameraCapture(Args args);
    ~LibcameraCapture();
    int fps() const;
    int width() const;
    int height() const;
    bool is_dma_capture() const;
    uint32_t format() const;
    Args config() const;

    static std::shared_ptr<LibcameraCapture> Create(Args args);
    LibcameraCapture &SetFormat(int width, int height);
    LibcameraCapture &SetFps(int fps = 30);
    LibcameraCapture &SetRotation(int angle);
    void StartCapture();

    std::shared_ptr<Observable<V4l2Buffer>> AsRawBufferObservable();
    std::shared_ptr<Observable<rtc::scoped_refptr<V4l2FrameBuffer>>> AsFrameBufferObservable();

  private:
    int fps_;
    int width_;
    int height_;
    int stride_;
    int buffer_count_;
    bool hw_accel_;
    uint32_t format_;
    Args config_;

    std::unique_ptr<libcamera::CameraManager> cm_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> camera_config_;
    libcamera::ControlList controls_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
    libcamera::Stream *stream_;
    std::map<int, std::vector<std::pair<void *, unsigned int>>> mappedBuffers_;

    rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer_;
    Subject<V4l2Buffer> raw_buffer_subject_;
    Subject<rtc::scoped_refptr<V4l2FrameBuffer>> frame_buffer_subject_;
    void NextBuffer(V4l2Buffer &raw_buffer);

    void Init(std::string device);
    void AllocateBuffer();
    void RequestComplete(libcamera::Request *request);
};

#endif
