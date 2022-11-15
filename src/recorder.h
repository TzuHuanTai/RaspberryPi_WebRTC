#ifndef RECODER_H_
#define RECODER_H_

#include "args.h"
#include "v4l2_utils.h"

#include "string"
#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class Recorder : public std::enable_shared_from_this<Recorder>
{
public:
    int width;
    int height;
    std::string filename;
    std::string base_path;
    std::string full_path;
    std::string encoder_name;
    AVRational frame_rate;

    Recorder(Args args);
    ~Recorder();

    void Write(Buffer buffer);
    void SetRotationNum(int max_num);

private:
    int video_frame_count_;
    AVFormatContext *fmt_ctx_;
    AVCodecContext* video_encoder_;
    AVStream *video_st_;

    std::string PrefixZero(int stc, int digits);
    std::string GenerateFilename();
    void CreateFormatContext(const char *extension);
    void AddVideoStream();
    void Finish();
};

#endif // RECODER_H_
