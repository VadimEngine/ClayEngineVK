
// standard lib
#include <stdexcept>
#include <iostream>
#include <chrono>

// third party
// assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// class
#include "clay/graphics/common/Model.h"


namespace clay {

Model::Model(IGraphicsContext& gContext)
 :mGraphicsContext_(gContext) {}

Model::~Model() {
    // vkDestroyBuffer(mGraphicsContext_.mDevice_, mIndexBuffer_, nullptr);
    // vkFreeMemory(mGraphicsContext_.mDevice_, mIndexBufferMemory_, nullptr);

    // vkDestroyBuffer(mGraphicsContext_.mDevice_, mVertexBuffer_, nullptr);
    // vkFreeMemory(mGraphicsContext_.mDevice_, mVertexBufferMemory_, nullptr);
}

void Model::parseModelFile(utils::FileData& fileData) {
//    Mesh* mesh = new Mesh(mGraphicsContext_);
//    mesh->parseModelFile(fileData);
//    mMeshes_.push_back(mesh);
}

void Model::update(float dt) {}

void Model::render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat) {
    // for now assume meshes and materials are paired by index
    for (int i = 0; i < mMeshes_.size(); ++i) {
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mMaterials_[i]->getPipeline());

        VkBuffer vertexBuffers[] = {mMeshes_[i]->mVertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(cmdBuffer, mMeshes_[i]->mIndexBuffer_, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            mMaterials_[i]->getPipelineLayout(),
            0,
            1,
            &mMaterials_[i]->mDescriptorSet_,
            0,
            nullptr
        );

        glm::mat4 modelMat = parentModelMat * getModelMatrix();
        
        vkCmdPushConstants(cmdBuffer, mMaterials_[i]->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMat);
        vkCmdPushConstants(cmdBuffer, mMaterials_[i]->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof(glm::vec4), &mColor_);

        vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(mMeshes_[i]->mIndices_.size()), 1, 0, 0, 0);
    }
}

glm::mat4 Model::getModelMatrix() {
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), mPosition_);
    //rotation matrix
    glm::mat4 rotationMat = glm::rotate(glm::mat4(1), glm::radians(mRotation_.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotationMat = glm::rotate(rotationMat, glm::radians(mRotation_.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotationMat = glm::rotate(rotationMat, glm::radians(mRotation_.z), glm::vec3(0.0f, 0.0f, 1.0f));
    // scale matrix
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), mScale_);

    return translationMat * rotationMat * scaleMat;
}

} // namespace clay