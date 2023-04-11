#include "h264_capture.h"
#include "encoder/raw_buffer.h"

// Linux
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// WebRTC
#include <api/video/video_frame_buffer.h>
#include <rtc_base/ref_counted_object.h>
#include <rtc_base/timestamp_aligner.h>

#include <iostream>

H264Capture::H264Capture(std::string device)
    : V4L2Capture(device) {}

rtc::scoped_refptr<V4L2Capture> H264Capture::Create(std::string device)
{
    auto capture = rtc::make_ref_counted<H264Capture>(device);
    return capture;
}

H264Capture &H264Capture::SetFormat(uint width, uint height, bool use_i420_src)
{
    width_ = width;
    height_ = height;
    use_i420_src_ = use_i420_src;
    capture_viedo_type_ = webrtc::VideoType::kUnknown;

    struct Buffer capture = {
        .name = "h264 capture",
        .width = width,
        .height = height,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE
    };

    std::cout << "Use h264 format source in v4l2" << std::endl;
    printf("  Width: %d\n"
           "  Height: %d\n",
           width, height);

    if (!V4l2Util::SetFormat(fd_, &capture, V4L2_PIX_FMT_H264))
    {
        exit(-1);
    }

    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE_MODE, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_HEADER_MODE, V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_PROFILE, V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_3_1);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, 12);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 2000000);

    return *this;
}

void H264Capture::OnFrameCaptured(Buffer buffer)
{
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> dst_buffer = nullptr;
    rtc::TimestampAligner timestamp_aligner_;
    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
        timestamp_aligner_.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
    if (!AdaptFrame(width_, height_, timestamp_us, &adapted_width, &adapted_height,
                    &crop_width, &crop_height, &crop_x, &crop_y))
    {
        return;
    }

    rtc::scoped_refptr<RawBuffer> raw_buffer(
        RawBuffer::Create(adapted_width, adapted_height, buffer.length));
    std::memcpy(raw_buffer->MutableData(),
                (uint8_t *)buffer.start,
                buffer.length);
    dst_buffer = raw_buffer;

    OnFrame(webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(dst_buffer)
                .set_rotation(webrtc::kVideoRotation_0)
                .set_timestamp_us(translated_timestamp_us)
                .build());
}
