#include "clay/application/common/BaseApp.h"

namespace clay {

BaseApp::BaseApp(BaseGraphicsContext* graphicsContext)
    : mResources_(*graphicsContext),
    mpGraphicsContext_(graphicsContext) {}

BaseApp::~BaseApp() {};

void BaseApp::loadResources() {}

BaseGraphicsContext& BaseApp::getGraphicsContext() {
    return *mpGraphicsContext_;
}

Resources& BaseApp::getResources() {
    return mResources_;
}

AudioManager& BaseApp::getAudioManager() {
    return mAudioManager_;
}

} // namespace clay