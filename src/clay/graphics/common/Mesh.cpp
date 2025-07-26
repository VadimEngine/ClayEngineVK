// standard lib
#include <stdexcept>
#include <chrono>
// third party
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// clay
#include "clay/utils/common/Logger.h"
// class
#include "clay/graphics/common/Mesh.h"

namespace clay {

Mesh processMesh(BaseGraphicsContext& gContext, aiMesh* mesh, const aiScene* scene) {
    std::vector<Mesh::Vertex> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Mesh::Vertex vertex;
        // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
        glm::vec3 vector;
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.position = vector;
        // normals
        if (mesh->HasNormals()) {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.normal = vector;
        } else {
            vertex.normal = {0,0,0};
        }
        // texture coordinates
        if(mesh->mTextureCoords[0]) { // does the mesh contain texture coordinates?
            glm::vec2 vec;
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
            // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.texCoord = vec;

        } else {
            vertex.texCoord = glm::vec2(0.0f, 0.0f);
        }

        if (mesh->HasTangentsAndBitangents()) {
            // tangent
            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;
            vertex.tangent = vector;
            // bitangent
            vector.x = mesh->mBitangents[i].x;
            vector.y = mesh->mBitangents[i].y;
            vector.z = mesh->mBitangents[i].z;
            vertex.bitangent = vector;
        } else {
            vertex.tangent = {0,0,0};
            vertex.bitangent = {0,0,0};
        }

        vertices.push_back(vertex);
    }

    // now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace face = mesh->mFaces[i];
        // retrieve all indices of the face and store them in the indices vector
        for(unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // TODO material/texture logic
    return {gContext, vertices, indices};
}

void processNode(BaseGraphicsContext& gContext, aiNode* node, const aiScene* scene, std::vector<Mesh>& meshList) {
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshList.push_back(processMesh(gContext, mesh, scene));
    }
    // then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(gContext, node->mChildren[i], scene, meshList);
    }
}

vk::VertexInputBindingDescription Mesh::Vertex::getBindingDescription() {
   return {
        .binding = 0,
        .stride = sizeof(Mesh::Vertex),
        .inputRate = vk::VertexInputRate::eVertex
    };
}

std::array<vk::VertexInputAttributeDescription, 5> Mesh::Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 5> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset = offsetof(Mesh::Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset = offsetof(Mesh::Vertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[2].offset = offsetof(Mesh::Vertex, texCoord);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[3].offset = offsetof(Mesh::Vertex, tangent);

    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[4].offset = offsetof(Mesh::Vertex, bitangent);

    return attributeDescriptions;
}

void Mesh::parseObjFile(BaseGraphicsContext& gContext, utils::FileData& fileData, std::vector<Mesh>& meshList) {
    Assimp::Importer import;
    const aiScene* scene = import.ReadFileFromMemory(
        fileData.data.get(),
        fileData.size,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace,
        "obj" // Pass a file extension if needed, e.g., "obj"
    );
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_E("ERROR::ASSIMP::%s", import.GetErrorString());
        return;
    }
    // Process the Assimp node and add to mMeshes_
    processNode(gContext, scene->mRootNode, scene, meshList);
}

Mesh::Mesh(BaseGraphicsContext& gContext)
    : mGraphicsContext_(gContext) {}

Mesh::Mesh(BaseGraphicsContext& gContext, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : mGraphicsContext_(gContext) {
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
}

// Move constructor
Mesh::Mesh(Mesh&& other) noexcept
    : mGraphicsContext_(other.mGraphicsContext_) {
    // Move other members
    mVertexBuffer_ = other.mVertexBuffer_;
    mVertexBufferMemory_ = other.mVertexBufferMemory_;
    mIndexBuffer_ = other.mIndexBuffer_;
    mIndexBufferMemory_ = other.mIndexBufferMemory_;
    mIndicesCount_ = other.mIndicesCount_;

    // Null out other's handles
    other.mVertexBuffer_ = nullptr;
    other.mVertexBufferMemory_ = nullptr;
    other.mIndexBuffer_ = nullptr;
    other.mIndexBufferMemory_ = nullptr;
}

// move assignment
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        finalize();
        mVertexBuffer_ = other.mVertexBuffer_;
        mVertexBufferMemory_ = other.mVertexBufferMemory_;
        mIndexBuffer_ = other.mIndexBuffer_;
        mIndexBufferMemory_ = other.mIndexBufferMemory_;

        other.mVertexBuffer_ = nullptr;
        other.mVertexBufferMemory_ = nullptr;
        other.mIndexBuffer_ = nullptr;
        other.mIndexBufferMemory_ = nullptr;
    }
    return *this;
}

Mesh::~Mesh() {
    finalize();
}

void Mesh::bindMesh(vk::CommandBuffer cmdBuffer) {
    vk::Buffer vertexBuffers[] = {mVertexBuffer_};
    vk::DeviceSize offsets[] = {0};
    cmdBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
    cmdBuffer.bindIndexBuffer(mIndexBuffer_, 0, vk::IndexType::eUint32);
}

void Mesh::createVertexBuffer(const std::vector<Vertex>& vertices) {
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
    mGraphicsContext_.getDevice().destroyBuffer(stagingBuffer);
    mGraphicsContext_.getDevice().freeMemory(stagingBufferMemory);
}

void Mesh::createIndexBuffer(const std::vector<unsigned int>& indices) {
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

    mGraphicsContext_.getDevice().destroyBuffer(stagingBuffer);
    mGraphicsContext_.getDevice().freeMemory(stagingBufferMemory);
}

vk::Buffer Mesh::getVertexBuffer() const {
    return mVertexBuffer_;
}

vk::Buffer Mesh::getIndexBuffer() const {
    return mIndexBuffer_;
}

uint32_t Mesh::getIndicesCount() const {
    return mIndicesCount_;
}

void Mesh::finalize() {
    if (mVertexBuffer_ != nullptr) {
        mGraphicsContext_.getDevice().destroyBuffer(mVertexBuffer_);
        mVertexBuffer_ = nullptr;
    }
    if (mVertexBufferMemory_ != nullptr) {
        mGraphicsContext_.getDevice().freeMemory(mVertexBufferMemory_);
        mVertexBufferMemory_ = nullptr;
    }

    if (mIndexBuffer_ != nullptr) {
        mGraphicsContext_.getDevice().destroyBuffer(mIndexBuffer_);
        mIndexBuffer_ = nullptr;
    }
    if (mIndexBufferMemory_ != nullptr) {
        mGraphicsContext_.getDevice().freeMemory(mIndexBufferMemory_);
        mIndexBufferMemory_ = nullptr;
    }
    mIndicesCount_ = 0;
}


} // namespace clay