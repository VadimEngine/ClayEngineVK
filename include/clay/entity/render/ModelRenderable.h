#pragma once
// clay
#include "clay/entity/render/BaseRenderable.h"
#include "clay/graphics/common/Model.h"

namespace clay {


class ModelRenderable : public BaseRenderable {
public:

    ModelRenderable(Model* pModel = nullptr);

    virtual ~ModelRenderable() = default;

    void render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat) override;

    const Model* getModel() const;

    void setModel(Model* pModel);

private:
    Model* mModel_;
};

} // namespace clay