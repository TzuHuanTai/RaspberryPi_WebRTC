#include "utils.h"

bool Utils::CreateFolder(const std::string& folder_path) {
    if (folder_path.empty()) {
        return false;
    }
    if (!std::filesystem::exists(folder_path)) {
        if (!std::filesystem::create_directory(folder_path)) {
            std::cerr << "Failed to create directory: " << folder_path << std::endl;
            return false;
        }
        std::cout << "Directory created: " << folder_path << std::endl;
    } else {
        std::cout << "Directory already exists: " << folder_path << std::endl;
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
