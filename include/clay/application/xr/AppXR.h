#pragma once
#ifdef CLAY_PLATFORM_XR
// standard lib
#include <chrono>
#include <list>
// clay
#include "clay/application/xr/XRSystem.h"
#include "clay/application/xr/XRSystem.h"
#include "clay/application/common/BaseApp.h"
#include "clay/application/common/BaseScene.h"
#include "clay/utils/xr/UtilsXR.h"
#include "clay/application/xr/InputHandlerXR.h"
#include "clay/graphics/xr/PanelRenderTargetXR.h"

namespace clay {

class AppXR : public BaseApp {
public:
    AppXR(XRSystem* system);

    virtual ~AppXR() = default;

    virtual void initialize();

    void Run();

    void setScene(BaseScene* newScene);

    InputHandlerXR& getInputHandler();

    // This frame's world-locked proj * view for eye 0 (left) / 1 (right). Valid during the
    // scene's render() (set just before it), e.g. for EntityManager::setCullingViewProj.
    const glm::mat4& getEyeViewProj(uint32_t eye) const { return mEyeViewProj_[eye < 2 ? eye : 1]; }

    AAssetManager* getAssetManager();

    // todo change with filepath instead of string
    utils::FileData loadFileToMemory_XR(const std::string& filePath);

    utils::ImageData loadImageFileToMemory_XR(const std::string &filePath);

    //private:

    virtual void CreateResources();

    // Called when the app starts or stops rendering: the XR session running AND the activity
    // resumed (it stops when the headset is taken off or the app leaves the foreground).
    // A place to pause/resume audio and the like.
    virtual void onSessionRunningChanged(bool running) {}

    void RenderFrame();

    bool RenderLayer(XRSystem::RenderLayerInfo &renderLayerInfo);

    void InitImguiRender();

public:
//private:

    std::list<std::unique_ptr<BaseScene>> mScenes_;

    InputHandlerXR mInputHandler_;
    // imgui: drawn each frame into this offscreen target, shown in the scene on a panel
    // (materials sample imguiImageView).
    PanelRenderTargetXR mImguiTarget_;
    vk::ImageView imguiImageView;
    // false: no ImGui panel (no full-size target, no per-frame GUI pass, scenes' renderGUI
    // isn't called) - for apps whose UI is UiComponentXR (RmlUi). Set before Run().
    bool mImguiPanelEnabled_ = true;

    uint32_t imguiWidth = 4128;
    uint32_t imguiHeight = 2208;
    XRSystem* mXRSystem_ = nullptr;
    // When the previous frame's scene update ran (for the dt scenes get).
    std::chrono::steady_clock::time_point mLastFrameTime_{};
    glm::mat4 mEyeViewProj_[2]{glm::mat4(1.0f), glm::mat4(1.0f)};

};

} // namespace clay

#endif // CLAY_PLATFORM_XR