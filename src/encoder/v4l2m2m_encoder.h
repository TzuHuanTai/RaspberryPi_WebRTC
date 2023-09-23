#ifndef V4L2M2M_ENCODER_H_
#define V4L2M2M_ENCODER_H_

#include "args.h"
#include "v4l2_utils.h"
#include "data_channel_subject.h"
#include "recorder.h"
#include "processor.h"

#include <functional>
#include <mutex>
#include <queue>

// WebRTC
#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include <rtc_base/platform_thread.h>

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

    void RegisterRecordingObserver(std::shared_ptr<Observable<char *>> observer,
                                   Args args);

    int32_t V4l2m2mConfigure(int width, int height, int fps);
    bool V4l2m2mEncode(const uint8_t *byte, uint32_t length, Buffer &buffer);
    void V4l2m2mRelease();
    bool OutputRawBuffer(const uint8_t *byte, uint32_t length);
    bool CaptureProcessedBuffer(Buffer &buffer);
    std::string name;

private:
    int fd_;
    int width_;
    int height_;
    int framerate_;
    int bitrate_bps_;
    int key_frame_interval_;
    int buffer_count_;
    bool is_recording_;
    bool is_capturing_;
    BufferGroup output_;
    BufferGroup capture_;
    std::mutex mtx_;
    std::mutex recording_mtx_;
    std::unique_ptr<Recorder> recorder_;
    std::unique_ptr<Processor> processor_;
    std::queue<int> output_buffer_queue_;
    std::queue<std::function<void(Buffer)>> sending_tasks_;
    Args args_;

    void WriteFile(Buffer encoded_buffer);
    void EnableRecorder(bool onoff);
    void SendFrame(const webrtc::VideoFrame &frame, Buffer &encoded_buffer);
    void CapturingFunction();

    webrtc::VideoCodec codec_;
    webrtc::EncodedImage encoded_image_;
    webrtc::EncodedImageCallback *callback_;
    rtc::PlatformThread capture_thread_;
    std::unique_ptr<webrtc::BitrateAdjuster> bitrate_adjuster_;
    std::shared_ptr<Observable<char *>> observer_;
};

#endif // V4L2M2M_ENCODER_H_
