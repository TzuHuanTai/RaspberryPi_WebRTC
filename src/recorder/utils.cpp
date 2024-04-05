#include "recorder/utils.h"

std::string RecUtil::PrefixZero(int src, int digits) {
    std::string str = std::to_string(src);
    std::string n_zero(digits - str.length(), '0');
    return n_zero + str;
}

std::string RecUtil::GenerateFilename() {
    time_t now = time(0);
    tm *ltm = localtime(&now);

    std::string year = RecUtil::PrefixZero(1900 + ltm->tm_year, 4);
    std::string month = RecUtil::PrefixZero(1 + ltm->tm_mon, 2);
    std::string day = RecUtil::PrefixZero(ltm->tm_mday, 2);
    std::string hour = RecUtil::PrefixZero(ltm->tm_hour, 2);
    std::string min = RecUtil::PrefixZero(ltm->tm_min, 2);
    std::string sec = RecUtil::PrefixZero(ltm->tm_sec, 2);

    std::stringstream s1;
    std::string filename;
    s1 << year << month << day << "_" << hour << min << sec;
    s1 >> filename;

    return filename;
}

AVFormatContext* RecUtil::CreateContainer(std::string record_path) {
    AVFormatContext* fmt_ctx = nullptr;
    std::string container = "mp4";
    auto filename = RecUtil::GenerateFilename() + "." + container;
    auto full_path = record_path + '/' + filename;

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
