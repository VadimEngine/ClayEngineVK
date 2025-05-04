#pragma once
#include <vector>
// clay
#include "clay/utils/common/Utils.h"
#include "clay/graphics/common/Material.h"
#include "clay/graphics/common/IGraphicsContext.h"
#include "clay/graphics/common/Camera.h"
#include "clay/graphics/common/Mesh.h"


namespace clay {

class Model {

public:
    Model(IGraphicsContext& gContext);

    ~Model();

    void parseModelFile(utils::FileData& fileData);

    void render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat);

    void update(float dt);

    glm::mat4 getModelMatrix();


public:

    IGraphicsContext& mGraphicsContext_;

    // for now just hold keep material and mesh list index matching
    std::vector<Mesh*> mMeshes_;
    std::vector<Material*> mMaterials_;

    glm::vec4 mColor_ = {1.0f, 1.0f, 1.0f, 1.0f};

    glm::vec3 mPosition_ = {0.0f, 0.0f, 0.0f};
    glm::vec3 mRotation_ = { 0.0f, 0.0f, 0.0f }; // TODO replace with orientation
    glm::vec3 mScale_ = { 1.0f, 1.0f, 1.0f };
};

} // namespace clay