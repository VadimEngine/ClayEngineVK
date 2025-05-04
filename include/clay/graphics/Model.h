#pragma once
#include <vector>
// clay
#include "clay/utils/Utils.h"
#include "clay/graphics/Material.h"


// for now just push all the vertices into 1 list but later do the right mesh pattern
// take in a binary file instead of loading it here to keep io and models separate

// TODO move to mesh
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();

    inline bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

class Model {

public:
    Model();

    ~Model();

    void parseModelFile(FileData& fileData);

    void createDescriptorSetLayout();

    Material* mMaterial_ = nullptr;

public:
    std::vector<Vertex> mVertices_;
    std::vector<uint32_t> mIndices_;

};