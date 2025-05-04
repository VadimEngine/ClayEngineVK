#pragma once
// standard lib
#include <array>
#include <memory>
#include <string>
// third party
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace clay::utils {
// TODO uint8_t instead of unsigned char?
struct FileData {
    std::unique_ptr<unsigned char[]> data;
    std::size_t size;
};

struct ImageData {
    // should this also be a unique ptr?
    std::unique_ptr<unsigned char[]> pixels;
    int width;
    int height;
    int channels;
};

} // namespace clay::utils