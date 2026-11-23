#pragma once
// standard lib
#include <memory>
#include <functional>
#include <string>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/application/common/Resources.h"
#include "clay/audio/AudioManager.h"

namespace clay {

// Reports loading progress to a loading screen: `progress` is 0..1, `label` is a
// short description of the step about to run (e.g. "Textures"). See
// BaseApp::loadResourcesWithProgress / AppDesktop's loading phase.
using LoadProgressFn = std::function<void(float progress, const std::string& label)>;

class BaseApp {
public:
    BaseApp(BaseGraphicsContext* graphicsContext);

    virtual ~BaseApp();

    virtual void loadResources();

    // Opt-in loading screen hooks (driven by AppDesktop::run before the main loop).
    // Override loadResourcesWithProgress to do heavy loading while calling
    // `report(progress, label)` periodically; the engine draws a progress bar between
    // calls. Override onResourcesLoaded to create the first scene once loading is done.
    // Both default to no-ops, so apps that load in their constructor are unaffected.
    virtual void loadResourcesWithProgress(const LoadProgressFn& report);
    virtual void onResourcesLoaded();

    Resources& getResources();

    AudioManager& getAudioManager();

    BaseGraphicsContext& getGraphicsContext();

protected:
    std::unique_ptr<BaseGraphicsContext> mpGraphicsContext_;

    Resources mResources_;

    AudioManager mAudioManager_;
};

} // namespace clay