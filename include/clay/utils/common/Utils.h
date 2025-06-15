#pragma once
// standard lib
#include <cstdint>
#include <memory>

namespace clay::utils {

struct FileData {
    std::unique_ptr<uint8_t[]> data;
    std::size_t size;
};

struct ImageData {
    // should this also be a unique ptr?
    std::unique_ptr<uint8_t[]> pixels;
    int width;
    int height;
    int channels;
};

} // namespace clay::utils