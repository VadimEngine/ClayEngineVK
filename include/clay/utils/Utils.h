#pragma once
// standard lib
#include <array>
#include <memory>
#include <string>
// third party
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>


struct FileData {
    std::unique_ptr<unsigned char[]> data;
    std::size_t size;
}; 

struct ImageData {
    // should this also be a unique ptr?
    unsigned char* pixels;
    int width;
    int height;
    int channels;
};

FileData loadFileToMemory(const std::string& filePath);