#pragma once
#ifdef CLAY_PLATFORM_ANDROID
// standard lib
#include <array>
#include <memory>
// third party
#include <android/input.h>
// project
#include "clay/application/common/BaseApp.h"
#include "clay/graphics/android/GraphicsContextAndroid.h"
#include "clay/graphics/common/PipelineResource.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/application/common/BaseScene.h"
#include "clay/application/android/InputHandlerAndroid.h"

namespace clay {

class AppAndroid : public BaseApp {
public:

    AppAndroid(android_app* app);

    ~AppAndroid();

    virtual void loadResources();

    void update();

    void render();

    void recordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex);

    void setScene(BaseScene* pScene);

    utils::FileData loadFileToMemory(const std::filesystem::path& filePath);

    utils::ImageData loadImageFileToMemory(const std::filesystem::path& filePath);

    /**
     * Handle Android input events
     * @param event The AInputEvent from Android
     * @return true if event was handled
     */
    bool handleInputEvent(AInputEvent* event);

    /**
     * Get the input handler
     * @return Reference to the input handler
     */
    InputHandlerAndroid& getInputHandler();

    /**
     * Called when the native window is recreated after being destroyed
     */
    void onWindowRecreated();

//protected
    std::array<std::unique_ptr<BaseScene>, 2> mSceneBuffer_;

    std::chrono::steady_clock::time_point mLastTime_;
    std::chrono::steady_clock::time_point mStartTime_;

    android_app* mAndroidNativeApp_;

    InputHandlerAndroid mInputHandler_;
};

} // namespace clay
#endif // CLAY_PLATFORM_ANDROID