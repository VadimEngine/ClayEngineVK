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
        Mesh* mesh; // TODO use id instead
        Material* material; // TODO use id instead
        glm::mat4 localTransform = glm::mat4(1); // TODO maybe replace with instance data(mode, color) that is dynamically sized
    };

    Model(BaseGraphicsContext& gContext);

    // move constructor
    Model(Model&& other) noexcept;

    // move assignment
    Model& operator=(Model&& other) noexcept;

    ~Model();

    void addElement(const ModelElement& element);

    void render(vk::CommandBuffer cmdBuffer, const void* userPushData, uint32_t userPushSize);

private:
    BaseGraphicsContext& mGraphicsContext_;
    std::vector<ModelElement> mModelGroups_;
};

} // namespace clay