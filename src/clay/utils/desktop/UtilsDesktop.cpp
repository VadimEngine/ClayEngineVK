#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <stdexcept>
#include <fstream>
// third party
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
// class
#include "clay/utils/desktop/UtilsDesktop.h"

namespace clay::utils {

FileData loadFileToMemory_desktop(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary |
                                 std::ios::ate); // 	seek to the end of stream immediately after open to get the file size
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filePath.string());
    }

    std::streamsize fileSize = file.tellg(); // get file size
    file.seekg(0, std::ios::beg); // move cursor back to begining

    auto buffer = std::make_unique<uint8_t[]>(fileSize);
    if (!file.read(reinterpret_cast<char *>(buffer.get()), fileSize)) {
        throw std::runtime_error("Failed to read file: " + filePath.string());
    }

    return {std::move(buffer), static_cast<std::size_t>(fileSize)};
}

ImageData loadImageFileToMemory_desktop(const std::filesystem::path& filePath) {
    auto fileData = loadFileToMemory_desktop(filePath);
    utils::ImageData imageData;

    imageData.pixels.reset(
        stbi_load_from_memory(
            fileData.data.get(),
            static_cast<int>(fileData.size),
            &imageData.width, &imageData.height, &imageData.channels,
            0
        )
    );

    return imageData;
}


} // namespace clay::utils

#endif // CLAY_PLATFORM_DESKTOP