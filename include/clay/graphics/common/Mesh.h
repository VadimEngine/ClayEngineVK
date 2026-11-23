#pragma once
// standard lib
#include <vector>
#include <string>
// third party
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
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
        // Per-vertex RGB color (OBJ "v x y z r g b" extension, read by assimp into
        // mColors[0]). Defaults to white so meshes/shaders that don't use it are
        // unaffected: a Flat shader multiplies its push-constant color by this.
        glm::vec3 color{1.0f};

        static vk::VertexInputBindingDescription getBindingDescription();

        static std::array<vk::VertexInputAttributeDescription, 6> getAttributeDescriptions();
    };

    // Parse an OBJ into submeshes (one per assimp aiMesh = object x material). If
    // outMaterialNames is non-null it is filled in PARALLEL with meshList, giving the
    // .mtl material name of each submesh (so callers can assign materials by real name
    // rather than guessing an index — assimp emits many meshes for a multi-object OBJ).
    static void parseObjFile(BaseGraphicsContext& gContext, utils::FileData& fileData,
                             std::vector<Mesh>& meshList,
                             std::vector<std::string>* outMaterialNames = nullptr);

    Mesh(BaseGraphicsContext& gContext);

    Mesh(BaseGraphicsContext& gContext, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);

    // move constructor
    Mesh(Mesh&& other) noexcept;

    // move assignment
    Mesh& operator=(Mesh&& other) noexcept;
    
    ~Mesh();

    void bindMesh(vk::CommandBuffer cmdBuffer);

    vk::Buffer getVertexBuffer() const;

    vk::Buffer getIndexBuffer() const;

    uint32_t getIndicesCount() const;

    // Local-space bounding sphere of the vertices: xyz = centre, w = radius. A negative
    // radius means "unknown" (mesh built without vertex data) and must never be culled.
    const glm::vec4& getBoundingSphere() const { return mBoundingSphere_; }

private:
    void createVertexBuffer(const std::vector<Vertex>& vertices);

    void createIndexBuffer(const std::vector<unsigned int>& indices);

    void finalize();

    // private:
    BaseGraphicsContext& mGraphicsContext_;

    uint32_t mIndicesCount_ = 0;
    glm::vec4 mBoundingSphere_{0.0f, 0.0f, 0.0f, -1.0f};
    vk::Buffer mVertexBuffer_{};
    vk::DeviceMemory mVertexBufferMemory_{};
    vk::Buffer mIndexBuffer_{};
    vk::DeviceMemory mIndexBufferMemory_{};
};

} // namespace clay