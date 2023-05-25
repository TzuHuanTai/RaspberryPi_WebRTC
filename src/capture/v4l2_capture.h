#ifndef V4L2_CAPTURER_H_
#define V4L2_CAPTURER_H_

#include "v4l2_utils.h"
#include "subject_interface.h"

#include <modules/video_capture/video_capture.h>
#include <rtc_base/platform_thread.h>
#include <rtc_base/synchronization/mutex.h>

class V4L2Capture : public SubjectInterface<Buffer>
{
private:
    int fd_;
    int camera_index_;
    std::string device_;
    int buffer_count_;
    bool capture_started;
    mutable webrtc::Mutex capture_lock_;
    BufferGroup capture_;
    Buffer shared_buffer_;

    rtc::PlatformThread capture_thread_;
    std::function<bool()> capture_func_;

    bool CheckMatchingDevice(std::string unique_name);
    int GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info);
    void CaptureThread();
    bool CaptureProcess();

public:
    int fps_;
    int width_;
    int height_;
    webrtc::VideoType capture_video_type_;

    V4L2Capture(std::string device);
    ~V4L2Capture();

    // SubjectInterface
    void Next(Buffer buffer) override;
    std::shared_ptr<Observable<Buffer>> AsObservable() override;
    void UnSubscribe() override;

    static std::shared_ptr<V4L2Capture> Create(std::string device);
    virtual V4L2Capture &SetFormat(uint width, uint height, std::string video_type);
    V4L2Capture &SetFps(uint fps = 30);
    V4L2Capture &SetRotation(uint angle);
    V4L2Capture &SetCaptureFunc(std::function<bool()> capture_func);
    void StartCapture();
    void CaptureImage();
    Buffer GetImage();
};

#endif
