#ifndef V4L2_CAPTURER_H_
#define V4L2_CAPTURER_H_

#include <modules/video_capture/video_capture.h>

#include "args.h"
#include "common/interface/subject.h"
#include "common/v4l2_frame_buffer.h"
#include "common/v4l2_utils.h"
#include "common/worker.h"
#include "v4l2_codecs/v4l2_decoder.h"
#include "v4l2_codecs/raw_frame_buffer.h"

class V4L2Capture {
  public:
    V4L2Capture(Args args);
    ~V4L2Capture();
    int fps() const;
    int width() const;
    int height() const;
    bool is_dma_capture() const;
    uint32_t format() const;
    Args config() const;
    webrtc::VideoType type() const;

    static std::shared_ptr<V4L2Capture> Create(Args args);
    V4L2Capture &SetFormat(int width, int height, std::string video_type);
    V4L2Capture &SetFps(int fps = 30);
    V4L2Capture &SetRotation(int angle);
    void StartCapture();

    std::shared_ptr<Observable<rtc::scoped_refptr<V4l2FrameBuffer>>> AsRawBufferObservable();
    std::shared_ptr<Observable<rtc::scoped_refptr<webrtc::VideoFrameBuffer>>> AsYuvBufferObservable();

  private:
    int fd_;
    int fps_;
    int width_;
    int height_;
    int buffer_count_;
    bool is_dma_;
    bool has_first_keyframe_;
    uint32_t format_;
    Args config_;
    V4l2BufferGroup capture_;
    webrtc::VideoType video_type_;
    std::unique_ptr<Worker> worker_;
    std::unique_ptr<V4l2Decoder> decoder_;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> yuv_buffer_frame_;
    Subject<rtc::scoped_refptr<V4l2FrameBuffer>> raw_buffer_subject_;
    Subject<rtc::scoped_refptr<webrtc::VideoFrameBuffer>> yuv_buffer_subject_;
    void NextBuffer(rtc::scoped_refptr<V4l2FrameBuffer> raw_buffer);

    void Init(std::string device);
    bool IsCompressedFormat() const;
    void CaptureImage();
    bool CheckMatchingDevice(std::string unique_name);
    int GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info);
};

#endif
