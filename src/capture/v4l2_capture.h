#ifndef V4L2_CAPTURER_H_
#define V4L2_CAPTURER_H_

#include "args.h"
#include "common/v4l2_utils.h"
#include "common/interface/subject.h"
#include "common/worker.h"

#include "mutex"

#include <modules/video_capture/video_capture.h>

class V4L2Capture : public Subject<Buffer>
{
public:
    V4L2Capture(Args args);
    ~V4L2Capture();
    int fps() const;
    int width() const;
    int height() const;
    bool is_dma() const;
    uint32_t format() const;
    webrtc::VideoType type();

    // Subject
    void Next(Buffer buffer) override;
    std::shared_ptr<Observable<Buffer>> AsObservable() override;
    void UnSubscribe() override;

    static std::shared_ptr<V4L2Capture> Create(Args args);
    V4L2Capture &SetFormat(int width, int height, std::string video_type);
    V4L2Capture &SetFps(int fps = 30);
    V4L2Capture &SetRotation(int angle);
    void StartCapture();
    const Buffer& GetImage() const;

private:
    int fd_;
    int fps_;
    int width_;
    int height_;
    int camera_index_;
    int buffer_count_;
    bool is_dma_;
    uint32_t format_;
    BufferGroup capture_;
    Buffer shared_buffer_;
    webrtc::VideoType video_type_;
    std::mutex capture_lock_;
    std::unique_ptr<Worker> worker_;

    void Init(std::string device);
    void CaptureImage();
    bool CheckMatchingDevice(std::string unique_name);
    int GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info);
};

#endif
