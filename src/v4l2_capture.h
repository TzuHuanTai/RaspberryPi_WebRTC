#ifndef V4L2CAPTURER_H_
#define V4L2CAPTURER_H_

#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_defines.h>
#include <modules/video_capture/video_capture_impl.h>
#include <rtc_base/platform_thread.h>
#include <rtc_base/synchronization/mutex.h>
#include <media/base/adapted_video_track_source.h>
#include <media/base/video_adapter.h>

struct Buffer
{
    void *start;
    uint length;
};

class V4L2Capture : public rtc::AdaptedVideoTrackSource
{
private:
    int fd_;
    int camera_index_;
    std::string device_;
    int buffer_count_ = 4;
    bool exit_ = false;
    bool captureStarted_ = false;
    webrtc::Mutex capture_lock_;

protected:
    uint width_;
    uint height_;

    Buffer *buffers_;
    webrtc::VideoType capture_viedo_type_ = webrtc::VideoType::kMJPEG;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> rtc_buffer;
    rtc::PlatformThread capture_thread_;
    std::function<bool()> capture_func_;

    bool AllocateBuffer();
    bool CheckMatchingDevice(std::string unique_name);
    int GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info);
    void CaptureThread();
    bool CaptureProcess();

public:
    V4L2Capture(std::string device);
    ~V4L2Capture();

    SourceState state() const override;
    bool remote() const override;
    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;
    void OnFrameCaptured(uint8_t *data, uint32_t bytesused);

    static rtc::scoped_refptr<V4L2Capture> Create(std::string device);
    V4L2Capture &SetFormat(uint width = 1920, uint height = 1080);
    V4L2Capture &SetFps(uint fps = 30);
    V4L2Capture &SetCaptureFunc(std::function<bool()> capture_func);
    void StartCapture();
    Buffer CaptureImage();
};

#endif