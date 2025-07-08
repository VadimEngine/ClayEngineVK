#pragma once
// standard lib
#include <vector>
// third party
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
// clay
#include "clay/utils/common/Utils.h"
#include "clay/graphics/common/Material.h"
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/Camera.h"

namespace clay {

// TODO template with vertex type?
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

    static void parseObjFile(BaseGraphicsContext& gContext, utils::FileData& fileData, std::vector<Mesh>& meshList);

    Mesh(BaseGraphicsContext& gContext);

    Mesh(BaseGraphicsContext& gContext, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);

    // move constructor
    Mesh(Mesh&& other) noexcept;

    // move assignment
    Mesh& operator=(Mesh&& other) noexcept;
    
    ~Mesh();

    void bindMesh(VkCommandBuffer cmdBuffer);

    VkBuffer getVertexBuffer() const;

    VkBuffer getIndexBuffer() const;

    uint32_t getIndicesCount() const;


private:
    void createVertexBuffer(const std::vector<Vertex>& vertices);

    void createIndexBuffer(const std::vector<unsigned int>& indices);

    void finalize();

    // private:
    BaseGraphicsContext& mGraphicsContext_;

    uint32_t mIndicesCount_ = 0;
    VkBuffer mVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory mVertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer mIndexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory mIndexBufferMemory_ = VK_NULL_HANDLE;
};

} // namespace clay