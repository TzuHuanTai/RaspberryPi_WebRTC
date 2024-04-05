#ifndef RECODER_MANAGER_H_
#define RECODER_MANAGER_H_

#include "capture/v4l2_capture.h"
#include "capture/pa_capture.h"
#include "recorder/audio_recorder.h"
#include "recorder/video_recorder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class RecorderManager {
public:
    static std::unique_ptr<RecorderManager> Create(
            std::shared_ptr<V4L2Capture> video_src, 
            std::shared_ptr<PaCapture> audio_src,
            std::string record_path);
    RecorderManager(std::string record_path);
    ~RecorderManager();
    void WriteIntoFile(AVPacket *pkt);
    void Start();
    void Stop();

protected:
    std::mutex mux;
    uint frame_count;
    std::string record_path;
    AVFormatContext *fmt_ctx;
    std::shared_ptr<Observable<Buffer>> observer;
    std::unique_ptr<VideoRecorder> video_recorder;
    std::unique_ptr<AudioRecorder> audio_recorder;

    void CreateVideoRecorder(std::shared_ptr<V4L2Capture> video_src, uint32_t format);
    void CreateAudioRecorder(std::shared_ptr<PaCapture> audio_src);
    void SubscribeVideoSource(std::shared_ptr<V4L2Capture> video_src);
};

#endif
