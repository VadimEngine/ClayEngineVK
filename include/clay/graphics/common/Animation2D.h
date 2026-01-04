#pragma once
// standard lib
#include <vector>
#include <cstdint>
// third party
#include <glm/vec4.hpp>
// clay
#include "clay/application/common/Handle.h"
#include "clay/graphics/common/Material.h"

namespace clay {

class Animation2D {
public:
    Handle<Material> materialHandle;       // Handle to spritesheet material
    std::vector<glm::vec4> frames;         // UV offsets for each frame
    float frameDuration = 0.15f;           // Seconds per frame
    bool loop = true;                      // Whether to loop the animation
};

} // namespace clay
