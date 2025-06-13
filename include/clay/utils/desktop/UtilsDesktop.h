#pragma once
#ifdef CLAY_PLATFORM_DESKTOP
// standard lib
#include <filesystem>
// clay
#include "clay/utils/common/Utils.h"

namespace clay::utils {

FileData loadFileToMemory_desktop(const std::filesystem::path& filePath);

ImageData loadImageFileToMemory_desktop(const std::filesystem::path& filePath);

} // namespace clay::utils

#endif // CLAY_PLATFORM_DESKTOP