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
#include "clay/entity/Entity.h"
#include "clay/application/common/Resources.h"

namespace clay {

class BaseApp;

class BaseScene {
public:
    struct CameraConstant {
        glm::mat4 view;
        glm::mat4 proj;
    };

    BaseScene(BaseApp& app);

    virtual ~BaseScene();

    virtual void initialize() = 0;

    virtual void render(VkCommandBuffer cmdBuffer) = 0;

    virtual void update(float dt) = 0;

    virtual void renderGUI(VkCommandBuffer cmdBuffer) = 0;

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