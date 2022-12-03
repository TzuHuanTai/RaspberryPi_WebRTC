#include "encoder/v4l2m2m_encoder.h"
#include "encoder/raw_buffer.h"

#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <memory>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <stdio.h>

const char *ENCODER_FILE = "/dev/video11";

V4l2m2mEncoder::V4l2m2mEncoder()
    : framerate_(30),
      key_frame_interval_(12),
      callback_(nullptr) {}

V4l2m2mEncoder::~V4l2m2mEncoder()
{
    V4l2m2mRelease();
}

int32_t V4l2m2mEncoder::InitEncode(
    const webrtc::VideoCodec *codec_settings,
    const VideoEncoder::Settings &settings)
{
    bitrate_adjuster_.reset(new webrtc::BitrateAdjuster(.5, .95));

    std::cout << "[V4l2m2mEncoder]: InitEncode" << std::endl;
    codec_ = *codec_settings;
    width_ = codec_settings->width;
    height_ = codec_settings->height;
    bitrate_bps_ = codec_settings->startBitrate * 1000;
    if (codec_settings->codecType == webrtc::kVideoCodecH264)
    {
        key_frame_interval_ = codec_settings->H264().keyFrameInterval;
    }
    std::cout << "[V4l2m2mEncoder]: width, " << width_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: height, " << height_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: framerate, " << framerate_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: bitrate_bps, " << bitrate_bps_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: key_frame_interval, " << key_frame_interval_ << std::endl;

    encoded_image_.timing_.flags = webrtc::VideoSendTiming::TimingFrameFlags::kInvalid;
    encoded_image_.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

    V4l2m2mConfigure(width_, height_, framerate_);

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4l2m2mEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback *callback)
{
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4l2m2mEncoder::Release()
{
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4l2m2mEncoder::Encode(
    const webrtc::VideoFrame &frame,
    const std::vector<webrtc::VideoFrameType> *frame_types)
{
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
        frame.video_frame_buffer();

    if (frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative)
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (codec_.codecType != webrtc::kVideoCodecH264)
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    RawBuffer *raw_buffer = static_cast<RawBuffer *>(frame_buffer.get());
    width_ = raw_buffer->width();
    height_ = raw_buffer->height();

    Buffer encoded_buffer =
        V4l2m2mEncode(raw_buffer->Data(), raw_buffer->Size());
    auto encoded_image_buffer =
        webrtc::EncodedImageBuffer::Create((uint8_t *)encoded_buffer.start, encoded_buffer.length);

    webrtc::CodecSpecificInfo codec_specific;
    codec_specific.codecType = webrtc::kVideoCodecH264;
    codec_specific.codecSpecific.H264.packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;

    encoded_image_.SetEncodedData(encoded_image_buffer);
    encoded_image_.SetTimestamp(frame.timestamp());
    encoded_image_.SetColorSpace(frame.color_space());
    encoded_image_._encodedWidth = width_;
    encoded_image_._encodedHeight = height_;
    encoded_image_.capture_time_ms_ = frame.render_time_ms();
    encoded_image_.ntp_time_ms_ = frame.ntp_time_ms();
    encoded_image_.rotation_ = frame.rotation();
    encoded_image_._frameType = webrtc::VideoFrameType::kVideoFrameDelta;

    if (encoded_buffer.flags & V4L2_BUF_FLAG_KEYFRAME)
    {
        encoded_image_._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    }

    auto result = callback_->OnEncodedImage(encoded_image_, &codec_specific);
    if (result.error != webrtc::EncodedImageCallback::Result::OK)
    {
        std::cout << "OnEncodedImage failed error:" << result.error << std::endl;
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    bitrate_adjuster_->Update(raw_buffer->Size());

    return WEBRTC_VIDEO_CODEC_OK;
}

void V4l2m2mEncoder::SetRates(const RateControlParameters &parameters)
{
    if (parameters.bitrate.get_sum_bps() <= 0 || parameters.framerate_fps <= 0)
    {
        return;
    }

    bitrate_adjuster_->SetTargetBitrateBps(parameters.bitrate.get_sum_bps());
    uint32_t adjusted_bitrate_bps_ = bitrate_adjuster_->GetAdjustedBitrateBps();

    if (bitrate_bps_ != adjusted_bitrate_bps_)
    {
        bitrate_bps_ = adjusted_bitrate_bps_;
        if (!V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, bitrate_bps_))
        {
            printf("Encoder failed set bitrate: %d bps\n", bitrate_bps_);
        }
    }

    if (framerate_ != parameters.framerate_fps)
    {
        framerate_ = parameters.framerate_fps;
        if (!V4l2Util::SetFps(fd_, output_.type, framerate_))
        {
            printf("Encoder failed set output fps: %d\n", framerate_);
        }
    }

    return;
}

webrtc::VideoEncoder::EncoderInfo V4l2m2mEncoder::GetEncoderInfo() const
{
    EncoderInfo info;
    info.supports_native_handle = true;
    info.implementation_name = "H264 V4L2m2m";
    return info;
}

int32_t V4l2m2mEncoder::V4l2m2mConfigure(int width, int height, int fps)
{
    fd_ = V4l2Util::OpenDevice(ENCODER_FILE);
    if (fd_ < 0)
    {
        exit(-1);
    }

    output_.name = "output";
    capture_.name = "capture";
    output_.width = capture_.width = width;
    output_.height = capture_.height = height;
    framerate_ = fps;

    if (!V4l2Util::InitBuffer(fd_, &output_, &capture_))
    {
        exit(-1);
    }

    /* set ext ctrls */
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE_MODE, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_HEADER_MODE, V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, width * height * fps / 10);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_PROFILE, V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_3_1);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, key_frame_interval_);

    if (!V4l2Util::SetFormat(fd_, &output_, V4L2_PIX_FMT_YUV420))
    {
        exit(-1);
    }

    if (!V4l2Util::SetFormat(fd_, &capture_, V4L2_PIX_FMT_H264))
    {
        exit(-1);
    }

    if (!V4l2Util::SetFps(fd_, output_.type, framerate_))
    {
        exit(-1);
    }

    if (!V4l2Util::AllocateBuffer(fd_, &output_) || !V4l2Util::AllocateBuffer(fd_, &capture_))
    {
        exit(-1);
    }

    if (ioctl(fd_, VIDIOC_QBUF, &output_.inner) < 0)
    {
        perror("ioctl Queue output Buffer");
        return false;
    }

    if (ioctl(fd_, VIDIOC_QBUF, &capture_.inner) < 0)
    {
        perror("ioctl Queue capture Buffer");
        return false;
    }

    std::cout << "V4l2m2m all prepare done" << std::endl;

    // turn on streaming
    V4l2Util::SwitchStream(fd_, &output_, true);
    V4l2Util::SwitchStream(fd_, &capture_, true);
    std::cout << "V4l2m2m stream on!" << std::endl;

    return 1;
}

Buffer V4l2m2mEncoder::V4l2m2mEncode(const uint8_t *byte, uint32_t length)
{
    struct v4l2_buffer buf = {0};
    struct v4l2_plane out_planes = {0};
    buf.memory = V4L2_MEMORY_MMAP;
    buf.length = 1;
    buf.m.planes = &out_planes;

    // Dequeue the output buffer, read the frame and queue it back.
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
    {
        perror("[V4l2m2mEncoder] ioctl dequeue output");
    }

    memcpy((uint8_t *)output_.start, byte, length);
    output_.length = length;

    if (ioctl(fd_, VIDIOC_QBUF, &output_.inner) < 0)
    {
        perror("[V4l2m2mEncoder] ioctl equeue output");
    }

    // Dequeue the capture buffer, write out the encoded frame and queue it back.
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
    {
        perror("[V4l2m2mEncoder] ioctl equeue capture");
    }

    struct Buffer buffer = {.start = capture_.start,
                            .length = buf.m.planes[0].bytesused,
                            .flags = buf.flags};

    if (ioctl(fd_, VIDIOC_QBUF, &capture_.inner) < 0)
    {
        perror("[V4l2m2mEncoder] ioctl queue capture");
    }

    return buffer;
}

void V4l2m2mEncoder::V4l2m2mRelease()
{
    munmap(output_.start, output_.length);
    munmap(capture_.start, capture_.length);

    // turn off stream
    V4l2Util::SwitchStream(fd_, &output_, false);
    V4l2Util::SwitchStream(fd_, &capture_, false);

    V4l2Util::CloseDevice(fd_);
    printf("[V4l2m2mEncoder]: fd(%d) is released\n", fd_);
}