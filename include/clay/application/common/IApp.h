#pragma once
// standard lib
#include <memory>
// clay
#include "clay/graphics/common/IGraphicsContext.h"
#include "clay/application/common/Resources.h"
#include "clay/audio/AudioManager.h"


namespace clay {

class IApp {
public:

    virtual Resources& getResources() = 0;

    virtual AudioManager& getAudioManager() = 0;

    std::unique_ptr<IGraphicsContext> mpGraphicsContext_;

};

} // namespace clay