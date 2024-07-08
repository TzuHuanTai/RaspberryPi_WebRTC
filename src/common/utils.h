#ifndef UTILS_
#define UTILS_

#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <sys/statvfs.h>

struct Buffer {
    void *start = nullptr;
    unsigned int length;
};

class Utils {
public:
    static std::string GenerateFilename();
    static std::string PrefixZero(int src, int digits);
    static std::string ToBase64(const std::string &binary_file);
    static std::string ReadFileInBinary(const std::string &file_path);
    static std::string FindLatestJpg(const std::string &directory);
    static bool CreateFolder(const std::string& folder_path);
    static bool CheckDriveSpace(const std::string &file_path, unsigned long min_free_space_mb);
    static Buffer ConvertYuvToJpeg(const uint8_t* yuv_data, int width, int height, int quality = 100);
    static void CreateJpegImage(const uint8_t* yuv_data, int width, int height,
                                std::string record_path, std::string filename);
    static void WriteJpegImage(Buffer buffer, std::string record_path, std::string filename);
};

#endif // UTILS_
