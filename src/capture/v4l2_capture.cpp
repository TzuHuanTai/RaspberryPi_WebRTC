#include "v4l2_capture.h"

// Linux
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/select.h>

// WebRTC
#include <modules/video_capture/video_capture_factory.h>

#include <iostream>

std::shared_ptr<V4L2Capture> V4L2Capture::Create(Args args) {
    auto ptr = std::make_shared<V4L2Capture>(args);
    ptr->Init(args.device);
    ptr->SetFps(args.fps)
        .SetRotation(args.rotation_angle)
        .SetFormat(args.width, args.height, args.v4l2_format)
        .StartCapture();
    return ptr;
}

V4L2Capture::V4L2Capture(Args args)
    : buffer_count_(2),
      is_dma_(args.enable_v4l2_dma) {}

void V4L2Capture::Init(std::string device) {
    webrtc::VideoCaptureModule::DeviceInfo *device_info = webrtc::VideoCaptureFactory::CreateDeviceInfo();
    fd_ = V4l2Util::OpenDevice(device.c_str());
    camera_index_ = GetCameraIndex(device_info);

    if (!V4l2Util::InitBuffer(fd_, &capture_, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP)) {
        exit(0);
    }
}

V4L2Capture::~V4L2Capture() {
    std::lock_guard<std::mutex> lock(capture_lock_);
    worker_.reset();
    V4l2Util::StreamOff(fd_, capture_.type);
    V4l2Util::DeallocateBuffer(fd_, &capture_);
    V4l2Util::CloseDevice(fd_);
}

int V4L2Capture::fps() const {
    return fps_;
}

int V4L2Capture::width() const {
    return width_;
}

int V4L2Capture::height() const {
    return height_;
}

bool V4L2Capture::is_dma() const {
    return is_dma_;
}

uint32_t V4L2Capture::format() const {
    return format_;
}

webrtc::VideoType V4L2Capture::type() {
    return video_type_;
}

void V4L2Capture::Next(Buffer buffer) {
    for (auto observer : observers_) {
        if (observer->subscribed_func_ != nullptr) {
            observer->subscribed_func_(buffer);
        }
    }
}

std::shared_ptr<Observable<Buffer>> V4L2Capture::AsObservable() {
    auto observer = std::make_shared<Observable<Buffer>>();
    observers_.push_back(observer);
    return observer;
}

void V4L2Capture::UnSubscribe() {
    auto it = observers_.begin();
    while (it != observers_.end())
    {
        it->reset();
        it = observers_.erase(it);
    }
}

bool V4L2Capture::CheckMatchingDevice(std::string unique_name) {
    struct v4l2_capability cap;
    if (V4l2Util::QueryCapabilities(fd_, &cap) && cap.bus_info[0] != 0
        && strcmp((const char *)cap.bus_info, unique_name.c_str()) == 0) {
        return true;
    }
    return false;
}

int V4L2Capture::GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info) {
    for (int i = 0; i < device_info->NumberOfDevices(); i++) {
        char device_name[256];
        char unique_name[256];
        if (device_info->GetDeviceName(static_cast<uint32_t>(i), device_name,
                                       sizeof(device_name), unique_name,
                                       sizeof(unique_name)) == 0
            && CheckMatchingDevice(unique_name)) {
            std::cout << "GetDeviceName(" << i
                      << "): device_name=" << device_name
                      << ", unique_name=" << unique_name << std::endl;
            return i;
        }
    }
    return -1;
}

V4L2Capture &V4L2Capture::SetFormat(int width, int height, std::string video_type) {
    width_ = width;
    height_ = height;

    if (video_type == "mjpeg") {
        std::cout << "Use mjpeg format source in v4l2" << std::endl;
        V4l2Util::SetFormat(fd_, &capture_, width, height, V4L2_PIX_FMT_MJPEG);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 10000000);
        format_ = V4L2_PIX_FMT_MJPEG;
        video_type_ = webrtc::VideoType::kMJPEG;
    } else if (video_type == "h264") {
        std::cout << "Use h264 format source in v4l2" << std::endl;
        V4l2Util::SetFormat(fd_, &capture_, width, height, V4L2_PIX_FMT_H264);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE_MODE, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_PROFILE, V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, 1); /* trick */
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 10000000);
        format_ = V4L2_PIX_FMT_H264;
        video_type_ = webrtc::VideoType::kUnknown;
    } else {
        std::cout << "Use yuv420(i420) format source in v4l2" << std::endl;
        V4l2Util::SetFormat(fd_, &capture_, width, height, V4L2_PIX_FMT_YUV420);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 10000000);
        format_ = V4L2_PIX_FMT_YUV420;
        video_type_ = webrtc::VideoType::kI420;
    }
    return *this;
}

V4L2Capture &V4L2Capture::SetFps(int fps) {
    fps_ = fps;
    printf("  Fps: %d\n", fps);
    if (!V4l2Util::SetFps(fd_, capture_.type, fps)) {
        exit(0);
    }
    return *this;
}

V4L2Capture &V4L2Capture::SetRotation(int angle) {
    printf("  Rotation: %d\n", angle);
    V4l2Util::SetCtrl(fd_, V4L2_CID_ROTATE, angle);
    return *this;
}

void V4L2Capture::CaptureImage() {
    std::lock_guard<std::mutex> lock(capture_lock_);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    timeval tv = {};
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    int r = select(fd_ + 1, &fds, NULL, NULL, &tv);

    if (r <= 0) { // timeout or failed
        return;
    }

    v4l2_buffer buf = {};
    buf.type = capture_.type;
    buf.memory = capture_.memory;

    if(!V4l2Util::DequeueBuffer(fd_, &buf)) {
        return;
    }

    shared_buffer_ = {.start = capture_.buffers[buf.index].start,
                      .length = buf.bytesused,
                      .flags = buf.flags};

    if(!V4l2Util::QueueBuffer(fd_, &buf)) {
        return;
    }

    Next(shared_buffer_);
}

const Buffer& V4L2Capture::GetImage() const {
    return shared_buffer_;
}

void V4L2Capture::StartCapture() {
    std::lock_guard<std::mutex> lock(capture_lock_);

    if (!V4l2Util::AllocateBuffer(fd_, &capture_, buffer_count_)
        || !V4l2Util::QueueBuffers(fd_, &capture_)) {
        exit(0);
    }

    V4l2Util::StreamOn(fd_, capture_.type);

    worker_.reset(new Worker([this]() { CaptureImage(); }));
    worker_->Run();
}
