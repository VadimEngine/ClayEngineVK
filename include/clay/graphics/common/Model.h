#pragma once
// standard lib
#include <vector>
#include <cstdint>

// third party
#include <glm/glm.hpp>

// clay
#include "clay/application/common/Handle.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/graphics/common/Material.h"

namespace clay {

class Model {

public:
    struct ModelElement {
        Handle<Mesh> meshHandle;
        Handle<Material> materialHandle;
        glm::mat4 localTransform = glm::mat4(1);
    };

    Model() = default;

    // move constructor
    Model(Model&& other) noexcept;

    // move assignment
    Model& operator=(Model&& other) noexcept;

    ~Model();

    void addElement(const ModelElement& element);

    const std::vector<ModelElement>& getElements() const { return mModelGroups_; }

private:
    std::vector<ModelElement> mModelGroups_;
};

} // namespace clay