#pragma once
#ifdef CLAY_PLATFORM_ANDROID
// standard lib
#include <array>
#include <memory>
// project
#include "clay/application/common/BaseApp.h"
#include "clay/graphics/android/GraphicsContextAndroid.h"
#include "clay/graphics/common/PipelineResource.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/application/common/BaseScene.h"

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

//protected
    std::array<std::unique_ptr<BaseScene>, 2> mSceneBuffer_;

    std::chrono::steady_clock::time_point mLastTime_;

    android_app* mAndroidNativeApp_;
};

} // namespace clay
#endif // CLAY_PLATFORM_ANDROID