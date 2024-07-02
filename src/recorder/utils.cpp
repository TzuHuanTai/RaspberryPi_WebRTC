#include "recorder/utils.h"

AVFormatContext* RecUtil::CreateContainer(std::string record_path, std::string filename) {
    AVFormatContext* fmt_ctx = nullptr;
    std::string container = "mp4";
    auto full_path = record_path + '/' + filename + "." + container;

    if (avformat_alloc_output_context2(&fmt_ctx, nullptr,
                                       container.c_str(),
                                       full_path.c_str()) < 0) {
        fprintf(stderr, "Could not alloc output context");
        return nullptr;
    }

    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, full_path.c_str(), AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open '%s'\n", full_path.c_str());
            return nullptr;
        }
    }
    av_dump_format(fmt_ctx, 0, full_path.c_str(), 1);

    return fmt_ctx;
}


void RecUtil::CreateThumbnail(std::string record_path, std::string filename) {
    // FFmpeg command to extract the first frame and save as thumbnail image.
    const std::string ffmpegCommand = 
        std::string("ffmpeg -xerror -loglevel quiet -hide_banner -y") +
        " -i " + record_path + "/" + filename + ".mp4" +
        " -vf \"select=eq(pict_type\\,I)\" -vsync vfr -frames:v 1 " +
        record_path + "/" + filename + ".jpg";
    printf("%s\n", ffmpegCommand.c_str());

    // Execute the command
    int result = std::system(ffmpegCommand.c_str());

    // Check the result
    if (result == 0) {
        printf("Thumbnail created successfully.\n");
    } else {
        printf("Error executing FFmpeg command.\n");
    }
}

bool RecUtil::WriteFormatHeader(AVFormatContext* fmt_ctx) {
    if (avformat_write_header(fmt_ctx, nullptr) < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        return false;
    }
    return true;
}

void RecUtil::CloseContext(AVFormatContext* fmt_ctx) {
    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
        avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
    }
}
