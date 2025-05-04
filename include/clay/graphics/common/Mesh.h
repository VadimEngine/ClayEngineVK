#pragma once
// standard lib
#include <vector>
// third party
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
// assimp TODO move these to cpp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// clay
#include "clay/utils/common/Utils.h"
#include "clay/graphics/common/Material.h"
#include "clay/graphics/common/IGraphicsContext.h"
#include "clay/graphics/common/Camera.h"

namespace clay {

class Mesh {
public:

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec3 tangent;
        glm::vec3 bitangent;

        static VkVertexInputBindingDescription getBindingDescription();

        static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
    };

    static void parseObjFile(IGraphicsContext& gContext, utils::FileData& fileData, std::vector<Mesh>& meshList);

    static void processNode(IGraphicsContext& gContext, aiNode *node, const aiScene *scene, std::vector<Mesh>& meshList);

    static Mesh processMesh(IGraphicsContext& graphicsAPI, aiMesh *mesh, const aiScene *scene);

    Mesh(IGraphicsContext& gContext);

    Mesh(IGraphicsContext& gContext, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);


    ~Mesh();

    void createVertexBuffer();

    void createIndexBuffer();

    // private:
    IGraphicsContext& mGraphicsContext_;

    std::vector<Vertex> mVertices_;
    std::vector<uint32_t> mIndices_;

    VkBuffer mVertexBuffer_;
    VkDeviceMemory mVertexBufferMemory_;
    VkBuffer mIndexBuffer_;
    VkDeviceMemory mIndexBufferMemory_;


};

} // namespace clay