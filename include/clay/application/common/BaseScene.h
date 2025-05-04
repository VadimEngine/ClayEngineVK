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
#include "clay/entity/render/ModelRenderable.h" // todo move this
#include "clay/utils/xr/UtilsXR.h"


namespace clay {

class IApp;

class BaseScene {
public:
    struct CameraConstant {
        glm::mat4 view;
        glm::mat4 proj;
    };

    BaseScene(IApp& app);

    virtual ~BaseScene();

    virtual void initialize() = 0;

    virtual void render(VkCommandBuffer cmdBuffer) = 0;

    virtual void update(float dt) = 0;

    virtual void renderGUI(VkCommandBuffer cmdBuffer) = 0;

    virtual void destroyResources() = 0;

    Camera* getFocusCamera();

//    virtual void update(float dt) = 0;

    CameraConstant cameraConstants;

    // temp for world locked
    CameraConstant cameraConstants2;


    Camera* mpFocusCamera_;

    Camera mCamera_;

    IApp& mApp_;
};

} // namespace clay