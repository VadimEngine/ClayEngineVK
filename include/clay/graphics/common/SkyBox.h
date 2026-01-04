#pragma once
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/graphics/common/Material.h"
#include "clay/application/common/Handle.h"

namespace clay {

class SkyBox {
public:

    SkyBox(clay::Handle<Mesh> meshHandle, clay::Handle<Material> materialHandle);
    ~SkyBox();

    void update(glm::quat& cameraOrientation);

    // Getters for rendering
    clay::Handle<Mesh> getMeshHandle() const { return mMeshHandle_; }
    clay::Handle<Material> getMaterialHandle() const { return mMaterialHandle_; }
    const glm::mat4& getModelMatrix() const { return mModelMat_; }

private:
    clay::Handle<Mesh> mMeshHandle_;
    clay::Handle<Material> mMaterialHandle_;
    glm::mat4 mModelMat_;
};


} // namespace clay