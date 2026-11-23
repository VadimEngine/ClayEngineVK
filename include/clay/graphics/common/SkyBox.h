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

    // Tint applied to the skybox (texture * color). Defaults to white, so a scene
    // that never sets it looks unchanged. Used e.g. for a day/night fade.
    const glm::vec4& getColor() const { return mColor_; }
    void setColor(const glm::vec4& color) { mColor_ = color; }

    // Swap the skybox texture/material in place (e.g. day <-> night) without
    // recreating the SkyBox, so its color/state is preserved.
    void setMaterialHandle(clay::Handle<Material> materialHandle) { mMaterialHandle_ = materialHandle; }

    // Extra fixed-world-axis rotation applied to the sky (post-multiplied after the
    // camera cancel, so it's a world rotation independent of where the camera looks).
    // Defaults to identity. Used e.g. to spin the night stars about the celestial pole.
    void setRotation(const glm::mat4& rotation) { mRotation_ = rotation; }

private:
    clay::Handle<Mesh> mMeshHandle_;
    clay::Handle<Material> mMaterialHandle_;
    glm::mat4 mModelMat_;
    glm::vec4 mColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::mat4 mRotation_ = glm::mat4(1.0f);
};


} // namespace clay