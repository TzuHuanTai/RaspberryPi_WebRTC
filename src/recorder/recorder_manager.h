#ifndef RECODER_MANAGER_H_
#define RECODER_MANAGER_H_

#include "capture/v4l2_capture.h"
#include "capture/pa_capture.h"
#include "recorder/audio_recorder.h"
#include "recorder/video_recorder.h"

#include <mutex>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
}

class RecorderManager {
public:
    static std::unique_ptr<RecorderManager> Create(
            std::shared_ptr<V4L2Capture> video_src, 
            std::shared_ptr<PaCapture> audio_src,
            std::string record_path);
    RecorderManager(std::shared_ptr<V4L2Capture> video_src, 
            std::shared_ptr<PaCapture> audio_src,
            std::string record_path);
    ~RecorderManager();
    void WriteIntoFile(AVPacket *pkt);
    void Start();
    void Stop();

protected:
    std::mutex ctx_mux;
    uint frame_count;
    uint fps;
    std::string record_path;
    AVFormatContext *fmt_ctx;
    bool has_first_keyframe;
    std::shared_ptr<Observable<Buffer>> video_observer;
    std::shared_ptr<Observable<PaBuffer>> audio_observer;
    std::shared_ptr<V4L2Capture> video_src; 
    std::shared_ptr<PaCapture> audio_src;
    std::unique_ptr<VideoRecorder> video_recorder;
    std::unique_ptr<AudioRecorder> audio_recorder;

    void CreateVideoRecorder(std::shared_ptr<V4L2Capture> video_src);
    void CreateAudioRecorder(std::shared_ptr<PaCapture> aduio_src);
    void SubscribeVideoSource(std::shared_ptr<V4L2Capture> video_src);
    void SubscribeAudioSource(std::shared_ptr<PaCapture> aduio_src);
};

#endif
