// clay::utils::convertRGBtoRGBA. Images are uploaded to the GPU as 4 bytes a pixel, so a
// 3-channel image (most JPEGs, some PNGs) goes through this first. A mistake here shows as
// skewed or miscoloured textures.
#include <gtest/gtest.h>
// standard lib
#include <cstdint>
#include <memory>
#include <stdexcept>
// clay
#include "clay/utils/common/Utils.h"

namespace {

clay::utils::ImageData makeImage(int width, int height, int channels) {
    clay::utils::ImageData image;
    image.width = width;
    image.height = height;
    image.channels = channels;
    const size_t bytes = size_t(width) * size_t(height) * size_t(channels);
    image.pixels = std::make_unique<uint8_t[]>(bytes);
    // A different value in every byte, so any shifted or dropped byte is noticed.
    for (size_t i = 0; i < bytes; ++i) image.pixels[i] = uint8_t(1 + (i * 7) % 250);
    return image;
}

} // namespace

TEST(UtilsTest, RgbToRgbaKeepsColoursAndAddsOpaqueAlpha) {
    const int width = 5, height = 3;  // (an odd width: rows are not padded)
    clay::utils::ImageData image = makeImage(width, height, 3);
    const clay::utils::ImageData original = makeImage(width, height, 3);

    clay::utils::convertRGBtoRGBA(image);

    EXPECT_EQ(image.channels, 4);
    EXPECT_EQ(image.width, width);
    EXPECT_EQ(image.height, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_EQ(image.pixels[pixel * 4 + 0], original.pixels[pixel * 3 + 0]) << "red, pixel " << pixel;
        EXPECT_EQ(image.pixels[pixel * 4 + 1], original.pixels[pixel * 3 + 1]) << "green, pixel " << pixel;
        EXPECT_EQ(image.pixels[pixel * 4 + 2], original.pixels[pixel * 3 + 2]) << "blue, pixel " << pixel;
        EXPECT_EQ(image.pixels[pixel * 4 + 3], 255) << "alpha, pixel " << pixel;
    }
}

TEST(UtilsTest, RgbToRgbaHandlesASinglePixel) {
    clay::utils::ImageData image = makeImage(1, 1, 3);
    image.pixels[0] = 10;
    image.pixels[1] = 20;
    image.pixels[2] = 30;
    clay::utils::convertRGBtoRGBA(image);
    EXPECT_EQ(image.pixels[0], 10);
    EXPECT_EQ(image.pixels[1], 20);
    EXPECT_EQ(image.pixels[2], 30);
    EXPECT_EQ(image.pixels[3], 255);
}

// Anything that is not 3 channels is refused, and left as it was: converting an image
// that is already RGBA would read past the meaning of its bytes.
TEST(UtilsTest, RgbToRgbaRefusesOtherChannelCounts) {
    for (const int channels : {1, 2, 4}) {
        clay::utils::ImageData image = makeImage(2, 2, channels);
        const uint8_t* before = image.pixels.get();
        EXPECT_THROW(clay::utils::convertRGBtoRGBA(image), std::runtime_error) << channels << " channels";
        EXPECT_EQ(image.channels, channels);
        EXPECT_EQ(image.pixels.get(), before);
    }
}
