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
        // The bone's own transform in the file's rest pose (relative to its parent node).
        // A clip that has no channel for this bone leaves it here.
        glm::mat4 localBind = glm::mat4(1.0f);
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

    // Tools (e.g. offline bakers) can ask meshes to keep a CPU copy of their vertex and
    // index data. Off by default: normal runs only need the GPU buffers. Applies to
    // meshes constructed after the switch is turned on.
    static void setKeepCpuCopies(bool keep) { sKeepCpuCopies_ = keep; }
    static bool keepCpuCopies() { return sKeepCpuCopies_; }
    // Empty unless setKeepCpuCopies(true) was in effect when this mesh was built.
    const std::vector<Vertex>& getCpuVertices() const { return mCpuVertices_; }
    const std::vector<unsigned int>& getCpuIndices() const { return mCpuIndices_; }
    // inverse(bones[i].offsetMatrix), precomputed: turns a palette entry back into the
    // bone's model-space transform (palette * inverseOffset) without a per-frame inverse.
    const glm::mat4& getInverseOffset(int boneIndex) const { return mInverseOffsets_[boneIndex]; }
    int getBoneIndex(const std::string& boneName) const;

    // Load animated mesh and animation from FBX file. When meshName is non-empty, the
    // sub-mesh whose name contains it is loaded (an FBX may hold several meshes sharing
    // one armature, e.g. hair styles); otherwise the first mesh is used.
    static std::unique_ptr<AnimatedMesh> loadFromFBX(
        BaseGraphicsContext& gContext,
        const std::string& filePath,
        SkeletalAnimation* outAnimation = nullptr,
        const std::string& meshName = ""
    );

    // Load EVERY mesh of an FBX as one skinned mesh, for rigs that are split into
    // several parts and coloured by material rather than by a texture (assimp emits
    // one sub-mesh per object x material). The parts share one bone list; a part that
    // is not skinned but parented to a bone (a held bow, a halo) rides that bone.
    // outPalette receives one colour per material (diffuse, or the emissive colour
    // when that is brighter), and each vertex's UV is set to its material's texel in
    // an N x 1 palette texture - so the mesh renders on any textured skinned pipeline
    // with a texture built from outPalette. outMaterialNames (optional) receives the
    // material names in palette order, so a caller can override individual colours.
    // keepTexCoords: leave each vertex its own UV (for a rig split into parts that all
    // share one real texture) instead of pointing it at its material's palette texel.
    // paletteFromVertexColours: for a rig painted with vertex colours (which skinned
    // vertices do not carry). Each triangle takes the average of its corners' colours and
    // becomes its own three vertices pointing at that colour's texel; outPalette is then
    // the distinct colours (as stored in the file) rather than one per material.
    static std::unique_ptr<AnimatedMesh> loadMergedFromFBX(
        BaseGraphicsContext& gContext,
        const std::string& filePath,
        std::vector<glm::vec4>& outPalette,
        std::vector<std::string>* outMaterialNames = nullptr,
        bool keepTexCoords = false,
        bool paletteFromVertexColours = false
    );

    // A transform applied above the root bones when a clip is evaluated on this mesh
    // (after the loader's fixed Z-up -> Y-up turn). The loaders only follow bones, so a
    // rotation / scale left on the FBX's armature object is lost: set it back here.
    const glm::mat4& getRootTransform() const { return mRootTransform_; }
    void setRootTransform(const glm::mat4& transform) { mRootTransform_ = transform; }
    // What the file itself has above the root bone (its armature object and any nodes over
    // it), as loaded. useFileArmatureTransform() makes its rotation and relative scale the
    // root transform (in place of the fixed Z-up -> Y-up turn): for rigs exported with the
    // armature turned or scaled.
    const glm::mat4& getFileArmatureTransform() const { return mFileArmatureTransform_; }
    void useFileArmatureTransform();

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
    std::vector<glm::mat4> mInverseOffsets_;
    glm::mat4 mRootTransform_ = glm::mat4(1.0f);
    glm::mat4 mFileArmatureTransform_ = glm::mat4(1.0f);
    std::vector<Vertex> mCpuVertices_;
    std::vector<unsigned int> mCpuIndices_;
    static inline bool sKeepCpuCopies_ = false;
    std::map<std::string, int> mBoneNameToIndex_;
};

} // namespace clay
