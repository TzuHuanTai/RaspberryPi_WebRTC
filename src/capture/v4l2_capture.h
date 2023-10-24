#ifndef V4L2_CAPTURER_H_
#define V4L2_CAPTURER_H_

#include "v4l2_utils.h"
#include "subject_interface.h"
#include "worker.h"

#include "mutex"

#include <modules/video_capture/video_capture.h>

class V4L2Capture : public SubjectInterface<Buffer>
{
public:
    V4L2Capture(std::string device);
    ~V4L2Capture();
    int fps() const;
    int width() const;
    int height() const;
    webrtc::VideoType type();

    // SubjectInterface
    void Next(Buffer buffer) override;
    std::shared_ptr<Observable<Buffer>> AsObservable() override;
    void UnSubscribe() override;

    static std::shared_ptr<V4L2Capture> Create(std::string device);
    V4L2Capture &SetFormat(int width, int height, std::string video_type);
    V4L2Capture &SetFps(int fps = 30);
    V4L2Capture &SetRotation(int angle);
    V4L2Capture &SetCaptureFunc(std::function<bool()> capture_func);
    void StartCapture();
    void CaptureImage();
    const Buffer& GetImage() const;

private:
    int fd_;
    int fps_;
    int width_;
    int height_;
    int camera_index_;
    int buffer_count_;
    BufferGroup capture_;
    Buffer shared_buffer_;
    webrtc::VideoType video_type_;
    std::mutex capture_lock_;

    std::function<void()> capture_func_;
    std::unique_ptr<Worker> worker_;

    bool CheckMatchingDevice(std::string unique_name);
    int GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info);
};

#endif
