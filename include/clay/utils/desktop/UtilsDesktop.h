#pragma once
#ifdef CLAY_PLATFORM_DESKTOP

#include "clay/utils/common/Utils.h"


namespace clay::utils {

FileData loadFileToMemory_desktop(const std::string& filePath);

} // namespace clay::utils

#endif // CLAY_PLATFORM_DESKTOP