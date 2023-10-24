#ifndef RECODER_H_
#define RECODER_H_

#include "args.h"
#include "v4l2_utils.h"

#include <string>
#include <future>
#include <queue>
#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class Recorder : public std::enable_shared_from_this<Recorder> {
public:
    int width;
    int height;
    std::string filename;
    std::string base_path;
    std::string full_path;
    std::string extension;
    std::string encoder_name;
    AVRational frame_rate;

    Recorder(Args config);
    ~Recorder();

    void PushEncodedBuffer(Buffer buffer);

private:
    bool is_recording_;
    int buffer_limit_num_;
    std::queue<Buffer> buffer_queue_;
    std::future<void> consumer_;

    int video_frame_count_;
    bool wait_first_keyframe_;
    AVFormatContext *fmt_ctx_;
    AVCodecContext *video_encoder_;
    AVStream *video_st_;

    std::string PrefixZero(int stc, int digits);
    std::string GenerateFilename();

    bool Initialize();
    void AddVideoStream();
    void ConsumeBuffer();
    void AsyncWriteFileBackground();
    bool Write(Buffer buffer);
    void Finish();
};

#endif // RECODER_H_
