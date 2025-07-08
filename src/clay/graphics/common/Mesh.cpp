// standard lib
#include <stdexcept>
#include <chrono>
// third party
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// class
#include "clay/graphics/common/Mesh.h"

namespace clay {

Mesh processMesh(BaseGraphicsContext& gContext, aiMesh* mesh, const aiScene* scene) {
    std::vector<Mesh::Vertex> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Mesh::Vertex vertex;
        glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
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

VkVertexInputBindingDescription Mesh::Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Mesh::Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 5> Mesh::Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Mesh::Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Mesh::Vertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Mesh::Vertex, texCoord);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Mesh::Vertex, tangent);

    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
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
        //LOG_E("ERROR::ASSIMP::%s", import.GetErrorString());
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
    other.mVertexBuffer_ = VK_NULL_HANDLE;
    other.mVertexBufferMemory_ = VK_NULL_HANDLE;
    other.mIndexBuffer_ = VK_NULL_HANDLE;
    other.mIndexBufferMemory_ = VK_NULL_HANDLE;
}

// move assignment
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        finalize();
        mVertexBuffer_ = other.mVertexBuffer_;
        mVertexBufferMemory_ = other.mVertexBufferMemory_;
        mIndexBuffer_ = other.mIndexBuffer_;
        mIndexBufferMemory_ = other.mIndexBufferMemory_;


        other.mVertexBuffer_ = VK_NULL_HANDLE;
        other.mVertexBufferMemory_ = VK_NULL_HANDLE;
        other.mIndexBuffer_ = VK_NULL_HANDLE;
        other.mIndexBufferMemory_ = VK_NULL_HANDLE;
    }
    return *this;
}

Mesh::~Mesh() {
    finalize();
}

void Mesh::bindMesh(VkCommandBuffer cmdBuffer) {
    VkBuffer vertexBuffers[] = {mVertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, mIndexBuffer_, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::createVertexBuffer(const std::vector<Vertex>& vertices) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    mGraphicsContext_.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(mGraphicsContext_.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t) bufferSize);
    vkUnmapMemory(mGraphicsContext_.getDevice(), stagingBufferMemory);

    mGraphicsContext_.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mVertexBuffer_,
        mVertexBufferMemory_
    );

    mGraphicsContext_.copyBuffer(stagingBuffer, mVertexBuffer_, bufferSize);

    vkDestroyBuffer(mGraphicsContext_.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(mGraphicsContext_.getDevice(), stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(const std::vector<unsigned int>& indices) {
    mIndicesCount_ = static_cast<uint32_t>(indices.size());
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    mGraphicsContext_.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(mGraphicsContext_.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(mGraphicsContext_.getDevice(), stagingBufferMemory);
    // use VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    mGraphicsContext_.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mIndexBuffer_,
        mIndexBufferMemory_
    );

    mGraphicsContext_.copyBuffer(stagingBuffer, mIndexBuffer_, bufferSize);

    vkDestroyBuffer(mGraphicsContext_.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(mGraphicsContext_.getDevice(), stagingBufferMemory, nullptr);
}

VkBuffer Mesh::getVertexBuffer() const {
    return mVertexBuffer_;
}

VkBuffer Mesh::getIndexBuffer() const {
    return mIndexBuffer_;
}

uint32_t Mesh::getIndicesCount() const {
    return mIndicesCount_;
}

void Mesh::finalize() {
    if (mVertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(mGraphicsContext_.getDevice(), mVertexBuffer_, nullptr);
        mVertexBuffer_ = VK_NULL_HANDLE;
    }
    if (mVertexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(mGraphicsContext_.getDevice(), mVertexBufferMemory_, nullptr);
        mVertexBufferMemory_ = VK_NULL_HANDLE;
    }

    if (mIndexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(mGraphicsContext_.getDevice(), mIndexBuffer_, nullptr);
        mIndexBuffer_ = VK_NULL_HANDLE;
    }
    if (mIndexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(mGraphicsContext_.getDevice(), mIndexBufferMemory_, nullptr);
        mIndexBufferMemory_ = VK_NULL_HANDLE;
    }
    mIndicesCount_ = 0;
}


} // namespace clay