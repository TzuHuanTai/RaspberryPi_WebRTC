#ifndef RECODER_UTILS_H_
#define RECODER_UTILS_H_

#include <chrono>
#include <sstream>
#include <string>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/audio_fifo.h>
}

class RecUtil {
public:
    // ffmpeg tool
    static AVFormatContext* CreateContainer(std::string record_path, std::string filename);
    static void CreateThumbnail(std::string record_path, std::string filename);
    static bool WriteFormatHeader(AVFormatContext* fmt_ctx);
    static void CloseContext(AVFormatContext* fmt_ctx);
};

#endif
