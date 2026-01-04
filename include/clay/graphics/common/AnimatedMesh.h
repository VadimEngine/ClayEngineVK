#pragma once
// standard lib
#include <vector>
#include <string>
#include <map>
#include <memory>
// third party
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>
// clay
#include <clay/graphics/common/BaseGraphicsContext.h>

namespace clay {

// Forward declaration
class SkeletalAnimation;

class AnimatedMesh {
public:
    // Vertex structure with bone data
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::ivec4 boneIds;     // indices of bones affecting this vertex
        glm::vec4 boneWeights;  // weight of each bone's influence

        static vk::VertexInputBindingDescription getBindingDescription();
        
        static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions();
    };

    // Bone information
    struct BoneInfo {
        std::string name;
        glm::mat4 offsetMatrix;  // Transforms from mesh space to bone space
        int parentIndex = -1;     // Index of parent bone (-1 for root)
    };

    AnimatedMesh(BaseGraphicsContext& gContext);

    AnimatedMesh(BaseGraphicsContext& gContext, 
                 const std::vector<Vertex>& vertices, 
                 const std::vector<unsigned int>& indices,
                 const std::vector<BoneInfo>& bones);

    // move constructor
    AnimatedMesh(AnimatedMesh&& other) noexcept;

    // move assignment
    AnimatedMesh& operator=(AnimatedMesh&& other) noexcept;
    
    ~AnimatedMesh();

    void bindMesh(vk::CommandBuffer cmdBuffer);

    vk::Buffer getVertexBuffer() const;
    vk::Buffer getIndexBuffer() const;
    uint32_t getIndicesCount() const;
    
    const std::vector<BoneInfo>& getBones() const { return mBones_; }
    int getBoneIndex(const std::string& boneName) const;

    // Load animated mesh and animation from FBX file
    static std::unique_ptr<AnimatedMesh> loadFromFBX(
        BaseGraphicsContext& gContext,
        const std::string& filePath,
        SkeletalAnimation* outAnimation = nullptr
    );

private:
    void createVertexBuffer(const std::vector<Vertex>& vertices);
    void createIndexBuffer(const std::vector<unsigned int>& indices);
    void finalize();

    BaseGraphicsContext& mGraphicsContext_;

    uint32_t mIndicesCount_ = 0;
    vk::Buffer mVertexBuffer_{};
    vk::DeviceMemory mVertexBufferMemory_{};
    vk::Buffer mIndexBuffer_{};
    vk::DeviceMemory mIndexBufferMemory_{};
    
    std::vector<BoneInfo> mBones_;
    std::map<std::string, int> mBoneNameToIndex_;
};

} // namespace clay
