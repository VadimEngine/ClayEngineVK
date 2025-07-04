// class
#include "clay/utils/common/Utils.h"
#include <stdexcept>

namespace clay::utils {

void convertRGBtoRGBA(ImageData& image) {
    if (image.channels != 3)
        throw std::runtime_error("convertRGBtoRGBA: input must have 3 channels");

    const std::size_t pixelCount = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);

    // Allocate new buffer: 4 bytes per pixel
    std::unique_ptr<uint8_t[]> rgba = std::make_unique<uint8_t[]>(pixelCount * 4);

    const uint8_t* src = image.pixels.get();
    uint8_t* dst = rgba.get();

    for (std::size_t i = 0; i < pixelCount; ++i) {
        // copy R,G,B
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        // set A
        dst[3] = 255;

        src += 3;
        dst += 4;
    }

    // Replace old data
    image.pixels   = std::move(rgba);
    image.channels = 4;
}

} // namespace clay::utils