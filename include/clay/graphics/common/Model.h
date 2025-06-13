#pragma once
// standard lib
#include <vector>
// clay
#include "clay/graphics/common/Material.h"
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/Camera.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/utils/common/Utils.h"

namespace clay {

class Model {

public:
    struct ModelElement {
        Mesh* mesh;
        Material* material;
        glm::mat4 localTransform = glm::mat4(1);
    };

    Model(BaseGraphicsContext& gContext);

    ~Model();

    void addElement(const ModelElement& element);

    void render(VkCommandBuffer cmdBuffer, const void* userPushData, uint32_t userPushSize);

private:
    BaseGraphicsContext& mGraphicsContext_;
    std::vector<ModelElement> mModelGroups_;
};

} // namespace clay