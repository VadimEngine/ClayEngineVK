#pragma once
// standard lib
#include <vector>
#include <memory>
// third party
#include <glm/glm.hpp>
// clay
#include "clay/utils/common/Utils.h"
#include "clay/graphics/common/Model.h"
#include "clay/graphics/common/Camera.h"
#include "clay/application/common/Resources.h"

namespace clay {

class BaseApp;

class BaseScene {
public:
    // One dynamic point light (e.g. a campfire). xyz = world position, w = reach radius;
    // rgb = colour, a = intensity. std140-friendly (two vec4s).
    struct GpuPointLight {
        glm::vec4 posRadius{0.0f};
        glm::vec4 color{0.0f};
    };
    struct CameraConstant {
        glm::mat4 view;
        glm::mat4 proj;
        // Dynamic point lights, read by lit fragment shaders for additive glow. x = count.
        glm::ivec4 lightCount{0};
        GpuPointLight lights[8];
        // The directional light (sun / moon), read by lit fragment shaders that declare
        // it: xyz = unit direction TOWARD the light, w = how strongly it shades and casts
        // shadows (1 = full, 0 = flat ambient only). Default: the fixed sun older scenes used.
        glm::vec4 sunLight{0.33686f, 0.84215f, 0.42108f, 1.0f};
    };

    BaseScene(BaseApp& app);

    virtual ~BaseScene();

    virtual void initialize() = 0;

    virtual void render(vk::CommandBuffer cmdBuffer) = 0;

    // Optional offscreen shadow-map pass, recorded before the main render pass
    // begins. Default: no-op. A scene overrides it to render its shadow casters
    // into the graphics context's shadow map (see GraphicsContextDesktop).
    virtual void renderShadowPass(vk::CommandBuffer cmdBuffer) {}

    virtual void update(float dt) = 0;

    virtual void renderGUI(vk::CommandBuffer cmdBuffer) = 0;

    virtual void destroyResources() = 0;

    BaseApp& getApp();

    Resources& getResources();

    Camera* getFocusCamera();

protected:
    BaseApp& mApp_;
    Resources mResources_;
    Camera mCamera_;
    Camera* mpFocusCamera_;
};

} // namespace clay