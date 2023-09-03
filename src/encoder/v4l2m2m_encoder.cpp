#include "encoder/v4l2m2m_encoder.h"

#include <sys/mman.h>
#include <iostream>
#include <memory>

const char *ENCODER_FILE = "/dev/video11";

V4l2m2mEncoder::V4l2m2mEncoder()
    : name_("h264_v4l2m2m"),
      framerate_(30),
      key_frame_interval_(12),
      buffer_count_(1),
      is_recording_(false),
      recorder_(nullptr),
      callback_(nullptr) {}

V4l2m2mEncoder::~V4l2m2mEncoder() {}

int32_t V4l2m2mEncoder::InitEncode(
    const webrtc::VideoCodec *codec_settings,
    const VideoEncoder::Settings &settings)
{
    bitrate_adjuster_.reset(new webrtc::BitrateAdjuster(1, 1));

    std::cout << "[V4l2m2mEncoder]: InitEncode" << std::endl;
    codec_ = *codec_settings;
    width_ = codec_settings->width;
    height_ = codec_settings->height;
    std::cout << "[V4l2m2mEncoder]: width, " << width_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: height, " << height_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: framerate, " << framerate_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: bitrate_bps, " << bitrate_bps_ << std::endl;
    std::cout << "[V4l2m2mEncoder]: key_frame_interval, " << key_frame_interval_ << std::endl;

    encoded_image_.timing_.flags = webrtc::VideoSendTiming::TimingFrameFlags::kInvalid;
    encoded_image_.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

    V4l2m2mConfigure(width_, height_, framerate_);
    EnableRecorder(is_recording_);

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
    std::lock_guard<std::mutex> lock(mtx_);

    recorder_.reset();

    V4l2m2mRelease();

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4l2m2mEncoder::Encode(
    const webrtc::VideoFrame &frame,
    const std::vector<webrtc::VideoFrameType> *frame_types)
{
    std::lock_guard<std::mutex> lock(mtx_);

    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
        frame.video_frame_buffer();

    if (codec_.codecType != webrtc::kVideoCodecH264)
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    auto i420_buffer = frame_buffer->GetI420();

    Buffer encoded_buffer = { 0 };
    int i420_buffer_size = (i420_buffer->StrideY() * height_) +
                    ((i420_buffer->StrideY() + 1) / 2) * ((height_ + 1) / 2) * 2;
    if (!V4l2m2mEncode(i420_buffer->DataY(), i420_buffer_size, encoded_buffer))
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

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

    WriteFile(encoded_buffer);

    auto result = callback_->OnEncodedImage(encoded_image_, &codec_specific);
    if (result.error != webrtc::EncodedImageCallback::Result::OK)
    {
        std::cout << "OnEncodedImage failed error:" << result.error << std::endl;
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    bitrate_adjuster_->Update(encoded_buffer.length);

    return WEBRTC_VIDEO_CODEC_OK;
}

void V4l2m2mEncoder::SetRates(const RateControlParameters &parameters)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (parameters.bitrate.get_sum_bps() <= 0 || parameters.framerate_fps <= 0 || fd_ < 0)
    {
        return;
    }

    bitrate_adjuster_->SetTargetBitrateBps(parameters.bitrate.get_sum_bps());
    uint32_t adjusted_bitrate_bps_ = bitrate_adjuster_->GetAdjustedBitrateBps();

    if(adjusted_bitrate_bps_ < 300000)
    {
        adjusted_bitrate_bps_ = 300000;
    }
    else
    {
        adjusted_bitrate_bps_ = (adjusted_bitrate_bps_ / 25000) * 25000;
    }

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
    info.implementation_name = name_;
    return info;
}

void V4l2m2mEncoder::EnableRecorder(bool onoff)
{
    std::lock_guard<std::mutex> lock(recording_mtx_);
    is_recording_ = onoff;
    if (onoff)
    {
        recorder_.reset(new Recorder(recoder_config_));
    }
    else
    {
        recorder_.reset();
    }
}

void V4l2m2mEncoder::RegisterRecordingObserver(std::shared_ptr<Observable<char *>> observer,
                                               std::string saving_path)
{
    observer_ = observer;

    recoder_config_ = (RecorderConfig){
        .fps = framerate_,
        .width = width_,
        .height = height_,
        .saving_path = saving_path,
        .container = "mp4",
        .encoder_name = name_};

    // TODO: parse incoming message into object to control recoder
    observer_->Subscribe(
        [&](char *message)
        {
            std::cout << "[V4l2m2mEncoder]: received msg => " << message << std::endl;
            if (strcmp(message, "true") == 0)
            {
                EnableRecorder(true);
                return;
            }
            EnableRecorder(false);
        });
}

void V4l2m2mEncoder::WriteFile(Buffer encoded_buffer)
{
    std::lock_guard<std::mutex> lock(recording_mtx_);
    if (recorder_ && is_recording_ && encoded_buffer.length > 0)
    {
        recorder_->PushBuffer(encoded_buffer);
    }
}

int32_t V4l2m2mEncoder::V4l2m2mConfigure(int width, int height, int fps)
{
    fd_ = V4l2Util::OpenDevice(ENCODER_FILE);
    if (fd_ < 0)
    {
        exit(-1);
    }

    output_.name = "v4l2_h264_encoder_output";
    capture_.name = "v4l2_h264_encoder_capture";
    output_.width = capture_.width = width;
    output_.height = capture_.height = height;
    framerate_ = fps;
    bitrate_bps_ = ((width * height * fps / 10) / 25000 + 1) * 25000;;

    if (!V4l2Util::InitBuffer(fd_, &output_, &capture_))
    {
        exit(-1);
    }

    /* set ext ctrls */
    // MODE_CBR will cause performance issue in high resolution?!
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE_MODE, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_HEADER_MODE, V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, bitrate_bps_);
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

    if (!V4l2Util::AllocateBuffer(fd_, &output_, buffer_count_) 
        || !V4l2Util::AllocateBuffer(fd_, &capture_, buffer_count_))
    {
        exit(-1);
    }

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);
    std::cout << "V4l2m2m all prepare done" << std::endl;

    return 1;
}

bool V4l2m2mEncoder::V4l2m2mEncode(const uint8_t *byte, uint32_t length, Buffer &buffer)
{
    struct v4l2_buffer buf = {0};
    struct v4l2_plane out_planes = {0};
    buf.memory = V4L2_MEMORY_MMAP;
    buf.length = 1;
    buf.m.planes = &out_planes;

    // Dequeue the output buffer, read the frame and queue it back.
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (!V4l2Util::DequeueBuffer(fd_, &buf))
    {
        return false;
    }

    memcpy((uint8_t *)output_.buffers[buf.index].start, byte, length);

    if (!V4l2Util::QueueBuffer(fd_, &output_.buffers[buf.index].inner))
    {
        return false;
    }

    // Dequeue the capture buffer, write out the encoded frame and queue it back.
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (!V4l2Util::DequeueBuffer(fd_, &buf))
    {
        return false;
    }

    buffer.start = capture_.buffers[buf.index].start;
    buffer.length = buf.m.planes[0].bytesused;
    buffer.flags = buf.flags;

    if (!V4l2Util::QueueBuffer(fd_, &capture_.buffers[buf.index].inner))
    {
        return false;
    }

    return true;
}

void V4l2m2mEncoder::V4l2m2mRelease()
{
    V4l2Util::StreamOff(fd_, output_.type);
    V4l2Util::StreamOff(fd_, capture_.type);

    V4l2Util::UnMap(&output_, buffer_count_);
    V4l2Util::UnMap(&capture_, buffer_count_);

    V4l2Util::CloseDevice(fd_);
    printf("[V4l2m2mEncoder]: fd(%d) is released\n", fd_);
}
