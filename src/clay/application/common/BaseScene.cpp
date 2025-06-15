// clay
#include "clay/application/common/BaseApp.h"
// class
#include "clay/application/common/BaseScene.h"

namespace clay {

BaseScene::BaseScene(BaseApp& app) : mApp_(app), mpFocusCamera_(&mCamera_) {}

BaseScene::~BaseScene() {}

BaseApp& BaseScene::getApp() {
    return mApp_;
}

Camera* BaseScene::getFocusCamera() {
    return mpFocusCamera_;
}

} // namespace clay
