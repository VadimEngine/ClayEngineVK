#pragma once
#ifdef CLAY_PLATFORM_XR
// standard lib
#include <list>
// clay
#include "clay/application/xr/XRSystem.h"
#include "clay/application/xr/XRSystem.h"
#include "clay/application/common/BaseApp.h"
#include "clay/application/common/BaseScene.h"
#include "clay/utils/xr/UtilsXR.h"
#include "clay/application/xr/InputHandlerXR.h"

namespace clay {

class AppXR : public BaseApp {
public:
    AppXR(XRSystem* system);

    virtual ~AppXR() = default;

    virtual void initialize();

    void Run();

    void setScene(BaseScene* newScene);

    InputHandlerXR& getInputHandler();

    AAssetManager* getAssetManager();

    // todo change with filepath instead of string
    utils::FileData loadFileToMemory_XR(const std::string& filePath);

    utils::ImageData loadImageFileToMemory_XR(const std::string &filePath);

    //private:

    virtual void CreateResources();

    void RenderFrame();

    bool RenderLayer(XRSystem::RenderLayerInfo &renderLayerInfo);

    void InitImguiRender();

public:
//private:

    std::list<std::unique_ptr<BaseScene>> mScenes_;

    InputHandlerXR mInputHandler_;
    // imgui
    VkCommandBuffer imguiCommandBuffer = VK_NULL_HANDLE;
    VkImage imguiImage = VK_NULL_HANDLE;
    VkDeviceMemory imguiImageMemory = VK_NULL_HANDLE;
    VkImageView imguiImageView = VK_NULL_HANDLE;
    VkFramebuffer imguiFrameBuffer = VK_NULL_HANDLE;

    uint32_t imguiWidth = 4128;
    uint32_t imguiHeight = 2208;
    XRSystem* mXRSystem_ = nullptr;
};

} // namespace clay

#endif // CLAY_PLATFORM_XR