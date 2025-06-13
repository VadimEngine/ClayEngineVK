#pragma once
// standard lib
#include <memory>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/application/common/Resources.h"
#include "clay/audio/AudioManager.h"

namespace clay {

class BaseApp {
public:
    BaseApp(BaseGraphicsContext* graphicsContext);

    virtual ~BaseApp();

    virtual void loadResources();

    Resources& getResources();

    AudioManager& getAudioManager();

    BaseGraphicsContext& getGraphicsContext();

protected:
    std::unique_ptr<BaseGraphicsContext> mpGraphicsContext_;

    Resources mResources_;

    AudioManager mAudioManager_;
};

} // namespace clay