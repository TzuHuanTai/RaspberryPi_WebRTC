#ifndef V4L2CAPTURER_H_
#define V4L2CAPTURER_H_

#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_defines.h>
#include <modules/video_capture/video_capture_impl.h>
#include <common_video/libyuv/include/webrtc_libyuv.h>

struct Buffer
{
    void *start;
    uint length;
};

class V4L2Capture
{
protected:
    int fd_;
    int camera_index_;
    std::string device_;
    int buffer_count_ = 4;
    Buffer *buffers_;
    bool AllocateBuffer();
    bool CheckMatchingDevice(std::string unique_name);
    int GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info);

public:
    V4L2Capture(std::string device);
    ~V4L2Capture();

    V4L2Capture &SetFormat(uint width = 1920, uint height = 1080);
    V4L2Capture &SetFps(uint fps = 30);
    void StartCapture();
    Buffer CaptureImage();
};

#endif