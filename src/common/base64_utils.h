#ifndef BASE64_UTILS_
#define BASE64_UTILS_

#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <filesystem>

class Base64Utils {
public:
    static std::string Encode(const std::string &binary);
    static std::string ReadBinary(const std::string &file_path);
    static std::string FindLatestJpg(const std::string &directory);
};

#endif // BASE64_UTILS_
