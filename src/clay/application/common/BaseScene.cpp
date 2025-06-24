// clay
#include "clay/application/common/BaseApp.h"
// class
#include "clay/application/common/BaseScene.h"

namespace clay {

BaseScene::BaseScene(BaseApp& app) 
    : mApp_(app),
      mResources_(app.getGraphicsContext()),
      mpFocusCamera_(&mCamera_) {}

BaseScene::~BaseScene() {}

BaseApp& BaseScene::getApp() {
    return mApp_;
}

Resources& BaseScene::getResources() {
    return mResources_;
}

Camera* BaseScene::getFocusCamera() {
    return mpFocusCamera_;
}

} // namespace clay
