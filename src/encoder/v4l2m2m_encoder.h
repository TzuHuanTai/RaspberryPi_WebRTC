#ifndef V4L2M2M_ENCODER_H_
#define V4L2M2M_ENCODER_H_

#include "v4l2_utils.h"
#include "data_channel_subject.h"
#include "recorder.h"

#include <mutex>
#include <memory>

// Linux
#include <linux/videodev2.h>

// WebRTC
#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include "rtc_base/synchronization/mutex.h"

class V4l2m2mEncoder : public webrtc::VideoEncoder
{
public:
    explicit V4l2m2mEncoder();
    ~V4l2m2mEncoder() override;

    int32_t InitEncode(const webrtc::VideoCodec *codec_settings,
                       const VideoEncoder::Settings &settings) override;
    int32_t RegisterEncodeCompleteCallback(
        webrtc::EncodedImageCallback *callback) override;
    int32_t Release() override;
    int32_t Encode(
        const webrtc::VideoFrame &frame,
        const std::vector<webrtc::VideoFrameType> *frame_types) override;
    void SetRates(const RateControlParameters &parameters) override;
    webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

    void RegisterRecordingObserver(std::shared_ptr<Observable> observer,
                                   std::string saving_path);

    int32_t V4l2m2mConfigure(int width, int height, int fps);
    Buffer V4l2m2mEncode(const uint8_t *byte, uint32_t length);
    void V4l2m2mRelease();

private:
    std::string name_;
    int fd_;
    int width_;
    int height_;
    int adapted_width_;
    int adapted_height_;
    int framerate_;
    int bitrate_bps_;
    int key_frame_interval_;
    Buffer output_;
    Buffer capture_;
    std::mutex mtx_;
    std::mutex recording_mtx_;
    Recorder *recorder_;
    RecorderConfig recoder_config_;

    void WriteFile(Buffer encoded_buffer);
    void EnableRecorder(bool onoff);

    webrtc::VideoCodec codec_;
    webrtc::EncodedImage encoded_image_;
    webrtc::EncodedImageCallback *callback_;
    std::unique_ptr<webrtc::BitrateAdjuster> bitrate_adjuster_;
    std::shared_ptr<Observable> observer_;
};

#endif // V4L2M2M_ENCODER_H_
