#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <stdexcept>
#include <fstream>
// class
#include "clay/utils/desktop/UtilsDesktop.h"


namespace clay::utils {

FileData loadFileToMemory(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    auto buffer = std::make_unique<unsigned char[]>(fileSize);
    if (!file.read(reinterpret_cast<char *>(buffer.get()), fileSize)) {
        throw std::runtime_error("Failed to read file: " + filePath);
    }

    return {std::move(buffer), static_cast<std::size_t>(fileSize)};
}

FileData loadFileToMemory_desktop(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary |
                                 std::ios::ate); // 	seek to the end of stream immediately after open to get the file size
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }

    std::streamsize fileSize = file.tellg(); // get file size
    file.seekg(0, std::ios::beg); // move cursor back to begining

    auto buffer = std::make_unique<unsigned char[]>(fileSize);
    if (!file.read(reinterpret_cast<char *>(buffer.get()), fileSize)) {
        throw std::runtime_error("Failed to read file: " + filePath);
    }

    return {std::move(buffer), static_cast<std::size_t>(fileSize)};
}

} // namespace clay::utils

#endif // CLAY_PLATFORM_DESKTOP