#include "utils.h"
#include "common/logging.h"

#include <third_party/libyuv/include/libyuv.h>
#include <jpeglib.h>

bool Utils::CreateFolder(const std::string& folder_path) {
    if (folder_path.empty()) {
        return false;
    }
    if (!std::filesystem::exists(folder_path)) {
        if (!std::filesystem::create_directory(folder_path)) {
            std::cerr << "Failed to create directory: " << folder_path << std::endl;
            return false;
        }
        DEBUG_PRINT("Directory created: %s", folder_path.c_str());
    } else {
        DEBUG_PRINT("Directory already exists: %s", folder_path.c_str());
    }
    return true;
}

std::string Utils::ToBase64(const std::string &binary_file) {
    std::string out;
    int val = 0, valb = -6;
    static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    for (unsigned char c : binary_file) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string Utils::ReadFileInBinary(const std::string &file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string Utils::FindLatestJpg(const std::string &directory) {
    std::string latest_file;
    auto latest_time = std::filesystem::file_time_type::min();

    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".jpg") {
            auto file_time = std::filesystem::last_write_time(entry);
            if (latest_file.empty() || file_time > latest_time) {
                latest_time = file_time;
                latest_file = entry.path().string();
            }
        }
    }

    if (latest_file.empty()) {
        throw std::runtime_error("No jpg files found in the directory");
    }

    return latest_file;
}

bool Utils::CheckDriveSpace(const std::string &file_path, unsigned long min_free_space_mb) {
    struct statvfs stat;
    if (statvfs(file_path.c_str(), &stat) != 0) {
        return false;
    }
    unsigned long free_space = stat.f_bsize * stat.f_bavail;
    unsigned long free_space_mb = free_space / (1024 * 1024);

    return free_space_mb >= min_free_space_mb;
}

std::string Utils::PrefixZero(int src, int digits) {
    std::string str = std::to_string(src);
    std::string n_zero(digits - str.length(), '0');
    return n_zero + str;
}

std::string Utils::GenerateFilename() {
    time_t now = time(0);
    tm *ltm = localtime(&now);

    std::string year = Utils::PrefixZero(1900 + ltm->tm_year, 4);
    std::string month = Utils::PrefixZero(1 + ltm->tm_mon, 2);
    std::string day = Utils::PrefixZero(ltm->tm_mday, 2);
    std::string hour = Utils::PrefixZero(ltm->tm_hour, 2);
    std::string min = Utils::PrefixZero(ltm->tm_min, 2);
    std::string sec = Utils::PrefixZero(ltm->tm_sec, 2);

    std::stringstream s1;
    std::string filename;
    s1 << year << month << day << "_" << hour << min << sec;
    s1 >> filename;

    return filename;
}

Buffer Utils::ConvertYuvToJpeg(const uint8_t* yuv_data, int width, int height, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    Buffer jpegBuffer = { NULL, 0 };

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    uint8_t* data = NULL;
    unsigned long size = 0;
    jpeg_mem_dest(&cinfo, &data, &size);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_EXT_BGR;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride = width * 3;
    uint8_t* rgb_data = (uint8_t*)malloc(width * height * 3);
    libyuv::I420ToRGB24(yuv_data, width,
                        yuv_data + width * height, width / 2,
                        yuv_data + width * height + (width * height / 4), width / 2,
                        rgb_data, width * 3,
                        width, height);

    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &rgb_data[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    free(rgb_data);

    jpegBuffer.start = data;
    jpegBuffer.length = size;

    return jpegBuffer;
}

void Utils::WriteJpegImage(Buffer buffer, std::string record_path, std::string filename) {
    std::string full_path = record_path + '/' + filename + ".jpg";
    FILE* file = fopen(full_path.c_str(), "wb");
    if (file) {
        fwrite((uint8_t*)buffer.start, 1, buffer.length, file);
        fclose(file);
        DEBUG_PRINT("JPEG data successfully written to %s", full_path.c_str());
    } else {
        ERROR_PRINT("Failed to open file for writing: %s", full_path.c_str());
    }
}

void Utils::CreateJpegImage(const uint8_t* yuv_data, int width, int height,
                            std::string record_path, std::string filename) {
    try {
        auto jpg_buffer = Utils::ConvertYuvToJpeg(yuv_data, width, height, 30);
        WriteJpegImage(jpg_buffer, record_path, filename);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
