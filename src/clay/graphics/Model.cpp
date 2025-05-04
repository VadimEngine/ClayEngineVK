
// standard lib
#include <stdexcept>
#include <iostream>
// third party
// assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// glm
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
// class
#include "clay/graphics/Model.h"

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
}

Model::Model() {
    
}

Model::~Model() {

}

void Model::parseModelFile(FileData& fileData) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFileFromMemory(
        fileData.data.get(),
        fileData.size,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace,
        "obj" // Pass a file extension if needed, e.g., "obj"
    );

    if (!scene || !scene->mRootNode) {
        throw std::runtime_error("Failed to load model: " + std::string(importer.GetErrorString()));
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    // Process each mesh in the model
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];

        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            Vertex vertex{};

            // Position
            vertex.pos = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

            // Texture coordinates (flip Y-axis to match your case)
            if (mesh->mTextureCoords[0]) {
                vertex.texCoord = glm::vec2(mesh->mTextureCoords[0][j].x, 1.0f - mesh->mTextureCoords[0][j].y);
            } else {
                vertex.texCoord = glm::vec2(0.0f, 0.0f); // Default if no texture coordinates are present
            }

            // Set color (default white)
            vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);

            // Ensure unique vertices using a hash map
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(mVertices_.size());
                mVertices_.push_back(vertex);
            }

            mIndices_.push_back(uniqueVertices[vertex]);
        }
    }

    std::cout << "Assimp Vertices: " << mVertices_.size() << std::endl;
}
