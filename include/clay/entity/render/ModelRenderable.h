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

    void setColor(const glm::vec4 newColor);

    glm::vec4 getColor() const;

private:
    Model* mModel_;
    glm::vec4 mColor_ = {1,1,1,1};
};

} // namespace clay