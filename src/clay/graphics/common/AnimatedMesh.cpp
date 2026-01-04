// standard lib
#include <stdexcept>
// third party
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// clay
#include <clay/utils/common/Logger.h>
#include <clay/graphics/common/AnimatedMesh.h>
#include <clay/graphics/common/SkeletalAnimation.h>

namespace clay {

vk::VertexInputBindingDescription AnimatedMesh::Vertex::getBindingDescription() {
   return {
        .binding = 0,
        .stride = sizeof(AnimatedMesh::Vertex),
        .inputRate = vk::VertexInputRate::eVertex
    };
}

std::array<vk::VertexInputAttributeDescription, 5> AnimatedMesh::Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 5> attributeDescriptions{};

    // Location 0: Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset = offsetof(AnimatedMesh::Vertex, position);

    // Location 1: Normal
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset = offsetof(AnimatedMesh::Vertex, normal);

    // Location 2: TexCoord
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[2].offset = offsetof(AnimatedMesh::Vertex, texCoord);

    // Location 3: BoneIds (ivec4)
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = vk::Format::eR32G32B32A32Sint;
    attributeDescriptions[3].offset = offsetof(AnimatedMesh::Vertex, boneIds);

    // Location 4: BoneWeights (vec4)
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = vk::Format::eR32G32B32A32Sfloat;
    attributeDescriptions[4].offset = offsetof(AnimatedMesh::Vertex, boneWeights);

    return attributeDescriptions;
}

AnimatedMesh::AnimatedMesh(BaseGraphicsContext& gContext)
    : mGraphicsContext_(gContext) {}

AnimatedMesh::AnimatedMesh(BaseGraphicsContext& gContext, 
                           const std::vector<Vertex>& vertices, 
                           const std::vector<unsigned int>& indices,
                           const std::vector<BoneInfo>& bones)
    : mGraphicsContext_(gContext)
    , mBones_(bones) {
    
    // Build bone name to index map
    for (size_t i = 0; i < mBones_.size(); ++i) {
        mBoneNameToIndex_[mBones_[i].name] = static_cast<int>(i);
    }
    
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
}

// Move constructor
AnimatedMesh::AnimatedMesh(AnimatedMesh&& other) noexcept
    : mGraphicsContext_(other.mGraphicsContext_) {
    // Move other members
    mVertexBuffer_ = other.mVertexBuffer_;
    mVertexBufferMemory_ = other.mVertexBufferMemory_;
    mIndexBuffer_ = other.mIndexBuffer_;
    mIndexBufferMemory_ = other.mIndexBufferMemory_;
    mIndicesCount_ = other.mIndicesCount_;
    mBones_ = std::move(other.mBones_);
    mBoneNameToIndex_ = std::move(other.mBoneNameToIndex_);

    // Null out other's handles
    other.mVertexBuffer_ = nullptr;
    other.mVertexBufferMemory_ = nullptr;
    other.mIndexBuffer_ = nullptr;
    other.mIndexBufferMemory_ = nullptr;
}

// move assignment
AnimatedMesh& AnimatedMesh::operator=(AnimatedMesh&& other) noexcept {
    if (this != &other) {
        finalize();
        mVertexBuffer_ = other.mVertexBuffer_;
        mVertexBufferMemory_ = other.mVertexBufferMemory_;
        mIndexBuffer_ = other.mIndexBuffer_;
        mIndexBufferMemory_ = other.mIndexBufferMemory_;
        mIndicesCount_ = other.mIndicesCount_;
        mBones_ = std::move(other.mBones_);
        mBoneNameToIndex_ = std::move(other.mBoneNameToIndex_);

        other.mVertexBuffer_ = nullptr;
        other.mVertexBufferMemory_ = nullptr;
        other.mIndexBuffer_ = nullptr;
        other.mIndexBufferMemory_ = nullptr;
    }
    return *this;
}

AnimatedMesh::~AnimatedMesh() {
    finalize();
}

void AnimatedMesh::bindMesh(vk::CommandBuffer cmdBuffer) {
    vk::Buffer vertexBuffers[] = {mVertexBuffer_};
    vk::DeviceSize offsets[] = {0};
    cmdBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
    cmdBuffer.bindIndexBuffer(mIndexBuffer_, 0, vk::IndexType::eUint32);
}

void AnimatedMesh::createVertexBuffer(const std::vector<Vertex>& vertices) {
    vk::DeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    mGraphicsContext_.createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data = mGraphicsContext_.getDevice().mapMemory(stagingBufferMemory, 0, bufferSize);
    memcpy(data, vertices.data(), (size_t) bufferSize);
    mGraphicsContext_.getDevice().unmapMemory(stagingBufferMemory);

    mGraphicsContext_.createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mVertexBuffer_,
        mVertexBufferMemory_
    );

    mGraphicsContext_.copyBuffer(stagingBuffer, mVertexBuffer_, bufferSize);
    mGraphicsContext_.getDevice().destroyBuffer(stagingBuffer, nullptr);
    mGraphicsContext_.getDevice().freeMemory(stagingBufferMemory, nullptr);
}

void AnimatedMesh::createIndexBuffer(const std::vector<unsigned int>& indices) {
    mIndicesCount_ = static_cast<uint32_t>(indices.size());
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    mGraphicsContext_.createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data = mGraphicsContext_.getDevice().mapMemory(stagingBufferMemory, 0, bufferSize);
    memcpy(data, indices.data(), (size_t) bufferSize);
    mGraphicsContext_.getDevice().unmapMemory(stagingBufferMemory);
    
    mGraphicsContext_.createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, 
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mIndexBuffer_,
        mIndexBufferMemory_
    );

    mGraphicsContext_.copyBuffer(stagingBuffer, mIndexBuffer_, bufferSize);

    mGraphicsContext_.getDevice().destroyBuffer(stagingBuffer, nullptr);
    mGraphicsContext_.getDevice().freeMemory(stagingBufferMemory, nullptr);
}

vk::Buffer AnimatedMesh::getVertexBuffer() const {
    return mVertexBuffer_;
}

vk::Buffer AnimatedMesh::getIndexBuffer() const {
    return mIndexBuffer_;
}

uint32_t AnimatedMesh::getIndicesCount() const {
    return mIndicesCount_;
}

int AnimatedMesh::getBoneIndex(const std::string& boneName) const {
    auto it = mBoneNameToIndex_.find(boneName);
    if (it != mBoneNameToIndex_.end()) {
        return it->second;
    }
    return -1;
}

void AnimatedMesh::finalize() {
    if (mVertexBuffer_) {
        mGraphicsContext_.getDevice().destroyBuffer(mVertexBuffer_, nullptr);
        mVertexBuffer_ = nullptr;
    }
    if (mVertexBufferMemory_) {
        mGraphicsContext_.getDevice().freeMemory(mVertexBufferMemory_, nullptr);
        mVertexBufferMemory_ = nullptr;
    }

    if (mIndexBuffer_) {
        mGraphicsContext_.getDevice().destroyBuffer(mIndexBuffer_, nullptr);
        mIndexBuffer_ = nullptr;
    }
    if (mIndexBufferMemory_) {
        mGraphicsContext_.getDevice().freeMemory(mIndexBufferMemory_, nullptr);
        mIndexBufferMemory_ = nullptr;
    }
    mIndicesCount_ = 0;
}

std::unique_ptr<AnimatedMesh> AnimatedMesh::loadFromFBX(
    BaseGraphicsContext& gContext,
    const std::string& filePath,
    SkeletalAnimation* outAnimation) {
    
    // Create Assimp importer
    Assimp::Importer importer;
    
    // Load the FBX file
    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_LimitBoneWeights
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_E("Assimp Error: %s", importer.GetErrorString());
        return nullptr;
    }
    
    if (scene->mNumMeshes == 0) {
        LOG_E("FBX file has no meshes");
        return nullptr;
    }
    
    // Use first mesh (can be extended to handle multiple meshes)
    const aiMesh* mesh = scene->mMeshes[0];
    
    // ==========================
    // PARSE VERTICES AND BONES
    // ==========================
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<BoneInfo> bones;
    
    // Initialize bone data for each vertex
    struct VertexBoneData {
        int boneIds[4] = {-1, -1, -1, -1};
        float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        int count = 0;
    };
    std::vector<VertexBoneData> vertexBoneData(mesh->mNumVertices);
    
    // Build bone list and offset matrices
    std::map<std::string, int> boneNameToIndex;
    for (unsigned int b = 0; b < mesh->mNumBones; b++) {
        const aiBone* bone = mesh->mBones[b];
        
        BoneInfo boneInfo;
        boneInfo.name = bone->mName.C_Str();
        
        // Convert offset matrix
        aiMatrix4x4 offsetMatrix = bone->mOffsetMatrix;
        boneInfo.offsetMatrix = glm::transpose(glm::make_mat4(&offsetMatrix.a1));
        
        boneNameToIndex[boneInfo.name] = static_cast<int>(bones.size());
        bones.push_back(boneInfo);
        
        // Store bone weights for vertices
        for (unsigned int w = 0; w < bone->mNumWeights; w++) {
            unsigned int vertexId = bone->mWeights[w].mVertexId;
            float weight = bone->mWeights[w].mWeight;
            
            if (vertexBoneData[vertexId].count < 4) {
                int idx = vertexBoneData[vertexId].count;
                vertexBoneData[vertexId].boneIds[idx] = b;
                vertexBoneData[vertexId].weights[idx] = weight;
                vertexBoneData[vertexId].count++;
            }
        }
    }
    
    // Build bone hierarchy (parent indices)
    std::function<void(aiNode*, int)> buildHierarchy = [&](aiNode* node, int parentIndex) {
        std::string nodeName = node->mName.C_Str();
        auto it = boneNameToIndex.find(nodeName);
        
        int currentBoneIndex = -1;
        if (it != boneNameToIndex.end()) {
            currentBoneIndex = it->second;
            bones[currentBoneIndex].parentIndex = parentIndex;
        }
        
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            buildHierarchy(node->mChildren[i], currentBoneIndex != -1 ? currentBoneIndex : parentIndex);
        }
    };
    buildHierarchy(scene->mRootNode, -1);
    
    // Parse vertex data
    for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
        Vertex vertex;
        
        // Position
        vertex.position = glm::vec3(
            mesh->mVertices[v].x,
            mesh->mVertices[v].y,
            mesh->mVertices[v].z
        );
        
        // Normal
        if (mesh->HasNormals()) {
            vertex.normal = glm::vec3(
                mesh->mNormals[v].x,
                mesh->mNormals[v].y,
                mesh->mNormals[v].z
            );
        } else {
            vertex.normal = glm::vec3(0.0f);
        }
        
        // Texture coordinates
        if (mesh->HasTextureCoords(0)) {
            vertex.texCoord = glm::vec2(
                mesh->mTextureCoords[0][v].x,
                mesh->mTextureCoords[0][v].y
            );
        } else {
            vertex.texCoord = glm::vec2(0.0f);
        }
        
        // Bone data
        for (int i = 0; i < 4; i++) {
            vertex.boneIds[i] = vertexBoneData[v].boneIds[i];
            vertex.boneWeights[i] = vertexBoneData[v].weights[i];
        }
        
        vertices.push_back(vertex);
    }
    
    // Parse indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // ==========================
    // PARSE ANIMATION
    // ==========================
    if (outAnimation && scene->mNumAnimations > 0) {
        const aiAnimation* animation = scene->mAnimations[0];
        
        SkeletalAnimation::AnimationClip clip;
        clip.name = animation->mName.C_Str();
        clip.duration = animation->mDuration;
        clip.ticksPerSecond = animation->mTicksPerSecond;
        
        for (unsigned int c = 0; c < animation->mNumChannels; c++) {
            const aiNodeAnim* nodeAnim = animation->mChannels[c];
            
            SkeletalAnimation::BoneChannel channel;
            channel.boneName = nodeAnim->mNodeName.C_Str();
            
            // Position keys
            for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys; i++) {
                SkeletalAnimation::PositionKey key;
                key.time = nodeAnim->mPositionKeys[i].mTime;
                key.value = glm::vec3(
                    nodeAnim->mPositionKeys[i].mValue.x,
                    nodeAnim->mPositionKeys[i].mValue.y,
                    nodeAnim->mPositionKeys[i].mValue.z
                );
                channel.positionKeys.push_back(key);
            }
            
            // Rotation keys
            for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys; i++) {
                SkeletalAnimation::RotationKey key;
                key.time = nodeAnim->mRotationKeys[i].mTime;
                key.value = glm::quat(
                    nodeAnim->mRotationKeys[i].mValue.w,
                    nodeAnim->mRotationKeys[i].mValue.x,
                    nodeAnim->mRotationKeys[i].mValue.y,
                    nodeAnim->mRotationKeys[i].mValue.z
                );
                channel.rotationKeys.push_back(key);
            }
            
            // Scale keys
            for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys; i++) {
                SkeletalAnimation::ScaleKey key;
                key.time = nodeAnim->mScalingKeys[i].mTime;
                key.value = glm::vec3(
                    nodeAnim->mScalingKeys[i].mValue.x,
                    nodeAnim->mScalingKeys[i].mValue.y,
                    nodeAnim->mScalingKeys[i].mValue.z
                );
                channel.scaleKeys.push_back(key);
            }
            
            clip.channels.push_back(channel);
        }
        
        outAnimation->setAnimationClip(clip);
    }
    
    return std::make_unique<AnimatedMesh>(gContext, vertices, indices, bones);
}

} // namespace clay
