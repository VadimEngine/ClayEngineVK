#pragma once
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/graphics/common/Material.h"

namespace clay {
// TODO add ECS
class SkyBox {
public:

    SkyBox(Mesh& mesh, Material& material);
    ~SkyBox();

    void update(glm::quat& cameraOrientation, float dt);

    void render(VkCommandBuffer cmdBuffer);

private:

    Mesh& mMesh_;
    Material& mMaterial_;
    glm::mat4 mModelMat_;
};


} // namespace clay