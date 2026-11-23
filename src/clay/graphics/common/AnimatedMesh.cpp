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
    mInverseOffsets_.reserve(mBones_.size());
    for (size_t i = 0; i < mBones_.size(); ++i) {
        mInverseOffsets_.push_back(glm::inverse(mBones_[i].offsetMatrix));
        mBoneNameToIndex_[mBones_[i].name] = static_cast<int>(i);
    }

    createVertexBuffer(vertices);
    createIndexBuffer(indices);
    if (sKeepCpuCopies_) {
        mCpuVertices_ = vertices;
        mCpuIndices_ = indices;
    }
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
    mInverseOffsets_ = std::move(other.mInverseOffsets_);
    mRootTransform_ = other.mRootTransform_;
    mFileArmatureTransform_ = other.mFileArmatureTransform_;
    mBoneNameToIndex_ = std::move(other.mBoneNameToIndex_);
    mCpuVertices_ = std::move(other.mCpuVertices_);
    mCpuIndices_ = std::move(other.mCpuIndices_);

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
        mInverseOffsets_ = std::move(other.mInverseOffsets_);
        mRootTransform_ = other.mRootTransform_;
    mFileArmatureTransform_ = other.mFileArmatureTransform_;
        mBoneNameToIndex_ = std::move(other.mBoneNameToIndex_);
        mCpuVertices_ = std::move(other.mCpuVertices_);
        mCpuIndices_ = std::move(other.mCpuIndices_);

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
    SkeletalAnimation* outAnimation,
    const std::string& meshName) {

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

    // Pick the mesh: by name (substring match) when requested, else the first one.
    unsigned int meshIdx = 0;
    if (!meshName.empty()) {
        bool found = false;
        for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
            if (std::string(scene->mMeshes[m]->mName.C_Str()).find(meshName) != std::string::npos) {
                meshIdx = m; found = true; break;
            }
        }
        if (!found) {
            LOG_E("FBX '%s' has no mesh matching '%s'", filePath.c_str(), meshName.c_str());
            return nullptr;
        }
    }
    const aiMesh* mesh = scene->mMeshes[meshIdx];

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
    // `above` is the accumulated transform of the nodes over this one that are not bones.
    glm::mat4 armatureTransform(1.0f);
    bool haveArmatureTransform = false;
    std::function<void(aiNode*, int, const glm::mat4&)> buildHierarchy = [&](aiNode* node, int parentIndex, const glm::mat4& above) {
        std::string nodeName = node->mName.C_Str();
        auto it = boneNameToIndex.find(nodeName);
        const glm::mat4 local = glm::transpose(glm::make_mat4(&node->mTransformation.a1));

        int currentBoneIndex = -1;
        if (it != boneNameToIndex.end()) {
            currentBoneIndex = it->second;
            bones[currentBoneIndex].parentIndex = parentIndex;
            bones[currentBoneIndex].localBind = local;
            if (parentIndex == -1 && !haveArmatureTransform) { armatureTransform = above; haveArmatureTransform = true; }
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            buildHierarchy(node->mChildren[i], currentBoneIndex != -1 ? currentBoneIndex : parentIndex,
                           (currentBoneIndex != -1 || parentIndex != -1) ? above : above * local);
        }
    };
    buildHierarchy(scene->mRootNode, -1, glm::mat4(1.0f));

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

    auto loaded = std::make_unique<AnimatedMesh>(gContext, vertices, indices, bones);
    loaded->mFileArmatureTransform_ = armatureTransform;
    return loaded;
}

void AnimatedMesh::useFileArmatureTransform() {
    // The armature's turn and its own scale are wanted; its position is just where the
    // model stood in the artist's scene. Scale: these files come out of the exporter with
    // the armature at 100 (a unit change the skin already accounts for), so 100 is "1x"
    // and a rig the artist scaled to 0.94 arrives as 94.
    const float kExporterScale = 100.0f;
    glm::mat4 turn(1.0f);
    for (int axis = 0; axis < 3; ++axis) {
        const glm::vec3 column(mFileArmatureTransform_[axis]);
        const float length = glm::length(column);
        if (length > 1e-6f) turn[axis] = glm::vec4(column / length * (length / kExporterScale), 0.0f);
    }
    // evaluate() applies Rx(-90) and then the root transform: cancel the former.
    mRootTransform_ = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * turn;
}

std::unique_ptr<AnimatedMesh> AnimatedMesh::loadMergedFromFBX(
    BaseGraphicsContext& gContext,
    const std::string& filePath,
    std::vector<glm::vec4>& outPalette,
    std::vector<std::string>* outMaterialNames,
    bool keepTexCoords,
    bool paletteFromVertexColours) {

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_LimitBoneWeights
    );
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode || scene->mNumMeshes == 0) {
        LOG_E("Assimp Error (%s): %s", filePath.c_str(), importer.GetErrorString());
        return nullptr;
    }

    auto toGlm = [](const aiMatrix4x4& m) { return glm::transpose(glm::make_mat4(&m.a1)); };

    // ---- palette: one colour per material ----
    outPalette.clear();
    if (outMaterialNames) outMaterialNames->clear();
    for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {
        if (outMaterialNames) outMaterialNames->push_back(scene->mMaterials[m]->GetName().C_Str());
        aiColor3D diffuse(0.6f, 0.6f, 0.6f), emissive(0.0f, 0.0f, 0.0f);
        scene->mMaterials[m]->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
        scene->mMaterials[m]->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
        outPalette.emplace_back(std::max(diffuse.r, emissive.r), std::max(diffuse.g, emissive.g),
                                std::max(diffuse.b, emissive.b), 1.0f);
    }
    const float paletteSize = static_cast<float>(std::max<size_t>(outPalette.size(), 1));

    // ---- shared bone list: the union of every part's bones ----
    std::vector<BoneInfo> bones;
    std::map<std::string, int> boneNameToIndex;
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
            const aiBone* bone = mesh->mBones[b];
            if (boneNameToIndex.count(bone->mName.C_Str())) continue;
            BoneInfo info;
            info.name = bone->mName.C_Str();
            info.offsetMatrix = toGlm(bone->mOffsetMatrix);
            boneNameToIndex[info.name] = static_cast<int>(bones.size());
            bones.push_back(info);
        }
    }
    std::function<void(aiNode*, int)> buildHierarchy = [&](aiNode* node, int parentIndex) {
        auto it = boneNameToIndex.find(node->mName.C_Str());
        int current = parentIndex;
        if (it != boneNameToIndex.end()) {
            current = it->second;
            bones[current].parentIndex = parentIndex;
            bones[current].localBind = toGlm(node->mTransformation);
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) buildHierarchy(node->mChildren[i], current);
    };
    buildHierarchy(scene->mRootNode, -1);

    // ---- geometry: walk the nodes so each part knows where it hangs ----
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // paletteFromVertexColours: the distinct colours found (6 bits a channel), in the
    // order met. While the mesh is built a vertex's texCoord.x holds its colour's index;
    // it becomes a real UV once the palette's size is known (below).
    std::vector<glm::vec4> colourPalette;
    std::map<uint32_t, int> colourIndex;
    const auto colourSlot = [&](float r, float g, float b) {
        const auto q = [](float c) { return static_cast<uint32_t>(std::min(std::max(c, 0.0f), 1.0f) * 63.0f + 0.5f); };
        const uint32_t key = (q(r) << 12) | (q(g) << 6) | q(b);
        auto it = colourIndex.find(key);
        if (it != colourIndex.end()) return it->second;
        const int slot = static_cast<int>(colourPalette.size());
        colourPalette.emplace_back(q(r) / 63.0f, q(g) / 63.0f, q(b) / 63.0f, 1.0f);
        colourIndex[key] = slot;
        return slot;
    };

    std::function<void(const aiNode*, const glm::mat4&)> visit = [&](const aiNode* node, const glm::mat4& parentGlobal) {
        const glm::mat4 global = parentGlobal * toGlm(node->mTransformation);
        for (unsigned int k = 0; k < node->mNumMeshes; ++k) {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[k]];
            const unsigned int base = static_cast<unsigned int>(vertices.size());
            const glm::vec2 uv((static_cast<float>(mesh->mMaterialIndex) + 0.5f) / paletteSize, 0.5f);

            // Skinned part: per-vertex weights, remapped to the shared bone list.
            struct Influence { int ids[4] = {-1, -1, -1, -1}; float w[4] = {0, 0, 0, 0}; int count = 0; };
            std::vector<Influence> influences(mesh->mNumVertices);
            for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
                const aiBone* bone = mesh->mBones[b];
                const int shared = boneNameToIndex[bone->mName.C_Str()];
                for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                    Influence& inf = influences[bone->mWeights[w].mVertexId];
                    if (inf.count < 4) { inf.ids[inf.count] = shared; inf.w[inf.count] = bone->mWeights[w].mWeight; ++inf.count; }
                }
            }

            // Rigid part: it follows the nearest bone above it. Move its vertices into
            // the space that bone's offset matrix expects, and bind them fully to it.
            int rigidBone = -1;
            glm::mat4 rigid(1.0f);
            if (mesh->mNumBones == 0) {
                glm::mat4 boneGlobal(1.0f);
                for (const aiNode* up = node->mParent; up; up = up->mParent) {
                    auto it = boneNameToIndex.find(up->mName.C_Str());
                    if (it == boneNameToIndex.end()) continue;
                    rigidBone = it->second;
                    // Global rest transform of that bone node.
                    std::vector<const aiNode*> chain;
                    for (const aiNode* n = up; n; n = n->mParent) chain.push_back(n);
                    for (auto r = chain.rbegin(); r != chain.rend(); ++r) boneGlobal = boneGlobal * toGlm((*r)->mTransformation);
                    break;
                }
                if (rigidBone >= 0) {
                    rigid = glm::inverse(bones[rigidBone].offsetMatrix) * glm::inverse(boneGlobal) * global;
                } else {
                    LOG_W("%s: part '%s' is neither skinned nor under a bone", filePath.c_str(), mesh->mName.C_Str());
                }
            }
            const glm::mat3 rigidNormal = glm::mat3(rigid);

            const auto makeVertex = [&](unsigned int v) {
                Vertex vertex;
                vertex.position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
                vertex.normal = mesh->HasNormals()
                    ? glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z) : glm::vec3(0.0f);
                vertex.texCoord = (keepTexCoords && mesh->HasTextureCoords(0))
                    ? glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y) : uv;
                if (rigidBone >= 0) {
                    vertex.position = glm::vec3(rigid * glm::vec4(vertex.position, 1.0f));
                    vertex.normal = glm::normalize(rigidNormal * vertex.normal);
                    vertex.boneIds = glm::ivec4(rigidBone, -1, -1, -1);
                    vertex.boneWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                } else {
                    for (int i = 0; i < 4; i++) {
                        vertex.boneIds[i] = influences[v].ids[i];
                        vertex.boneWeights[i] = influences[v].w[i];
                    }
                }
                return vertex;
            };
            if (paletteFromVertexColours) {
                // One colour a triangle (its corners' average), each triangle its own vertices.
                const glm::vec4& fallback = outPalette[std::min<size_t>(mesh->mMaterialIndex, outPalette.size() - 1)];
                for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                    const aiFace& face = mesh->mFaces[f];
                    if (face.mNumIndices != 3) continue;
                    glm::vec3 colour(fallback);
                    if (mesh->HasVertexColors(0)) {
                        colour = glm::vec3(0.0f);
                        for (unsigned int j = 0; j < 3; ++j) {
                            const aiColor4D& c = mesh->mColors[0][face.mIndices[j]];
                            colour += glm::vec3(c.r, c.g, c.b) / 3.0f;
                        }
                    }
                    const float slot = static_cast<float>(colourSlot(colour.r, colour.g, colour.b));
                    for (unsigned int j = 0; j < 3; ++j) {
                        Vertex vertex = makeVertex(face.mIndices[j]);
                        vertex.texCoord = glm::vec2(slot, 0.5f);
                        indices.push_back(static_cast<unsigned int>(vertices.size()));
                        vertices.push_back(vertex);
                    }
                }
            } else {
                for (unsigned int v = 0; v < mesh->mNumVertices; ++v) vertices.push_back(makeVertex(v));
                for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                    const aiFace& face = mesh->mFaces[f];
                    for (unsigned int j = 0; j < face.mNumIndices; ++j) indices.push_back(base + face.mIndices[j]);
                }
            }
        }
        for (unsigned int i = 0; i < node->mNumChildren; ++i) visit(node->mChildren[i], global);
    };
    visit(scene->mRootNode, glm::mat4(1.0f));

    if (paletteFromVertexColours && !colourPalette.empty()) {
        outPalette = colourPalette;
        const float count = static_cast<float>(colourPalette.size());
        for (Vertex& vertex : vertices) vertex.texCoord.x = (vertex.texCoord.x + 0.5f) / count;
    }

    LOG_I("%s: merged %u parts, %zu vertices, %zu bones, %zu materials",
          filePath.c_str(), scene->mNumMeshes, vertices.size(), bones.size(), outPalette.size());
    if (vertices.empty()) return nullptr;
    return std::make_unique<AnimatedMesh>(gContext, vertices, indices, bones);
}

} // namespace clay
