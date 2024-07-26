#include "logging.h"

std::string GetFileName(const std::string &file_path) {
    size_t start_pos = file_path.find_last_of("/\\") + 1;
    size_t end_pos = file_path.find_last_of(".");
    if (start_pos == std::string::npos) {
        start_pos = 0;
    }
    if (end_pos == std::string::npos || end_pos < start_pos) {
        end_pos = file_path.length();
    }
    return file_path.substr(start_pos, end_pos - start_pos);
}
