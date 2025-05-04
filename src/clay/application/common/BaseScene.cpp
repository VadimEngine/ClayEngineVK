// standard lib
#include <iostream>
// third party
// glm
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
// assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// imgui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
// clay
#include "clay/application/common/IApp.h"
// class
#include "clay/application/common/BaseScene.h"

namespace clay {


BaseScene::BaseScene(IApp& app) : mApp_(app), mpFocusCamera_(&mCamera_) {}

BaseScene::~BaseScene() {}

Camera* BaseScene::getFocusCamera() {
    return mpFocusCamera_;
}


} // namespace clay
