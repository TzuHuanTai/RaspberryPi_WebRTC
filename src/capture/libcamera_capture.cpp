#include "libcamera_capture.h"

#include <sys/mman.h>

#include "common/logging.h"

std::shared_ptr<LibcameraCapture> LibcameraCapture::Create(Args args) {
    auto ptr = std::make_shared<LibcameraCapture>(args);
    ptr->Init(args.device);
    ptr->SetFps(args.fps)
        .SetRotation(args.rotation_angle)
        .SetFormat(args.width, args.height)
        .StartCapture();
    return ptr;
}

LibcameraCapture::LibcameraCapture(Args args)
    : buffer_count_(4),
      hw_accel_(args.hw_accel),
      format_(V4L2_PIX_FMT_YUV420),
      config_(args) {}

void LibcameraCapture::Init(std::string device) {
    cm_ = std::make_unique<libcamera::CameraManager>();
    cm_->start();

    if (cm_->cameras().size() == 0) {
        ERROR_PRINT("No camera is available via libcamera.");
    }

    std::string cameraId = cm_->cameras()[0]->id();
    INFO_PRINT("camera id: %s", cameraId.c_str());
    camera_ = cm_->get(cameraId);
    camera_->acquire();
    camera_config_ = camera_->generateConfiguration({libcamera::StreamRole::VideoRecording});
}

LibcameraCapture::~LibcameraCapture() {
    camera_->stop();
    allocator_->free(stream_);
    camera_->release();
    camera_.reset();
    cm_->stop();
}

int LibcameraCapture::fps() const { return fps_; }

int LibcameraCapture::width() const { return width_; }

int LibcameraCapture::height() const { return height_; }

bool LibcameraCapture::is_dma_capture() const { return hw_accel_; }

uint32_t LibcameraCapture::format() const { return format_; }

Args LibcameraCapture::config() const { return config_; }

LibcameraCapture &LibcameraCapture::SetFormat(int width, int height) {
    DEBUG_PRINT("camera original format: %s", camera_config_->at(0).toString().c_str());

    if (width && height) {
        libcamera::Size size(width, height);
        camera_config_->at(0).size = size;
    }

    camera_config_->at(0).pixelFormat = libcamera::formats::YUV420;
    camera_config_->at(0).bufferCount = buffer_count_;

    auto validation = camera_config_->validate();
    if (validation == libcamera::CameraConfiguration::Status::Valid) {
        INFO_PRINT("camera validated format: %s.", camera_config_->at(0).toString().c_str());
    } else if (validation == libcamera::CameraConfiguration::Status::Adjusted) {
        INFO_PRINT("camera adjusted format: %s.", camera_config_->at(0).toString().c_str());
    } else {
        ERROR_PRINT("Failed to validate camera configuration.");
        exit(-1);
    }

    width_ = camera_config_->at(0).size.width;
    height_ = camera_config_->at(0).size.height;
    stride_ = camera_config_->at(0).stride;

    INFO_PRINT("  width: %d, height: %d, stride: %d", width_, height_, stride_);

    return *this;
}

LibcameraCapture &LibcameraCapture::SetFps(int fps) {
    fps_ = fps;
    int64_t frame_time = 1000000 / fps;
    controls_.set(libcamera::controls::FrameDurationLimits,
                  libcamera::Span<const int64_t, 2>({frame_time, frame_time}));
    DEBUG_PRINT("  Fps: %d", fps);

    return *this;
}

LibcameraCapture &LibcameraCapture::SetRotation(int angle) {
    if (angle == 90) {
        camera_config_->orientation = libcamera::Orientation::Rotate90;
    } else if (angle == 180) {
        camera_config_->orientation = libcamera::Orientation::Rotate180;
    } else if (angle == 270) {
        camera_config_->orientation = libcamera::Orientation::Rotate270;
    }

    DEBUG_PRINT("  Rotation: %d", angle);

    return *this;
}

void LibcameraCapture::AllocateBuffer() {
    allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);

    stream_ = camera_config_->at(0).stream();
    int ret = allocator_->allocate(stream_);
    if (ret < 0) {
        ERROR_PRINT("Can't allocate buffers");
    }

    auto &buffers = allocator_->buffers(stream_);
    if (buffer_count_ != buffers.size()) {
        ERROR_PRINT("Buffer counts not match allocated buffer number");
        exit(-1);
    }

    for (unsigned int i = 0; i < buffer_count_; i++) {
        auto &buffer = buffers[i];
        int fd = 0;
        int buffer_length = 0;
        for (auto &plane : buffer->planes()) {
            fd = plane.fd.get();
            buffer_length += plane.length;
        }
        void *memory = mmap(NULL, buffer_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        mappedBuffers_[fd].push_back(std::make_pair(memory, buffer_length));
        DEBUG_PRINT("Allocated fd(%d) Buffer[%d] pointer: %p, length: %d", fd, i, memory,
                    buffer_length);

        auto request = camera_->createRequest();
        if (!request) {
            ERROR_PRINT("Can't create camera request");
        }
        int ret = request->addBuffer(stream_, buffer.get());
        if (ret < 0) {
            ERROR_PRINT("Can't set buffer for request");
        }
        requests_.push_back(std::move(request));
    }
}

void LibcameraCapture::RequestComplete(libcamera::Request *request) {
    if (request->status() == libcamera::Request::RequestCancelled) {
        return;
    }

    auto &buffers = request->buffers();
    int fd = 0;
    int length = 0;
    timeval tv = {};

    auto *buffer = buffers.begin()->second;
    for (unsigned int i = 0; i < buffer->planes().size(); ++i) {
        auto &plane = buffer->planes()[i];
        fd = plane.fd.get();
        length += plane.length;
        void *data = mappedBuffers_[fd][i].first;
        tv.tv_sec = buffer->metadata().timestamp / 1000000000;
        tv.tv_usec = (buffer->metadata().timestamp % 1000000000) / 1000;
    }

    V4l2Buffer v4l2_buffer((uint8_t *)mappedBuffers_[fd][0].first, length, V4L2_BUF_FLAG_KEYFRAME,
                           tv);
    NextBuffer(v4l2_buffer);

    request->reuse(libcamera::Request::ReuseBuffers);
    camera_->queueRequest(request);
}

void LibcameraCapture::NextBuffer(V4l2Buffer &buffer) {
    frame_buffer_ = V4l2FrameBuffer::Create(width_, height_, buffer, format_);
    frame_buffer_subject_.Next(frame_buffer_);
    raw_buffer_subject_.Next(buffer);
}

std::shared_ptr<Observable<V4l2Buffer>> LibcameraCapture::AsRawBufferObservable() {
    return raw_buffer_subject_.AsObservable();
}

std::shared_ptr<Observable<rtc::scoped_refptr<V4l2FrameBuffer>>>
LibcameraCapture::AsFrameBufferObservable() {
    return frame_buffer_subject_.AsObservable();
}

void LibcameraCapture::StartCapture() {
    int ret = camera_->configure(camera_config_.get());
    if (ret < 0) {
        ERROR_PRINT("Failed to configure camera");
        exit(-1);
    }

    AllocateBuffer();

    camera_->requestCompleted.connect(this, &LibcameraCapture::RequestComplete);

    ret = camera_->start(&controls_);
    if (ret) {
        ERROR_PRINT("Failed to start capture");
        exit(-1);
    }

    controls_.clear();

    for (auto &request : requests_) {
        ret = camera_->queueRequest(request.get());
        if (ret < 0) {
            ERROR_PRINT("Can't queue request");
            camera_->stop();
        }
    }
}
