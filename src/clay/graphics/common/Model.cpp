// class
#include "clay/graphics/common/Model.h"

namespace clay {

// TODO load from file logic

Model::Model(BaseGraphicsContext& gContext)
    :mGraphicsContext_(gContext) {}

    // move constructor
Model::Model(Model&& other) noexcept
    : mGraphicsContext_(other.mGraphicsContext_) {
        mModelGroups_ = other.mModelGroups_;
    }

    // move assignment
Model& Model::operator=(Model&& other) noexcept {
    mModelGroups_ = other.mModelGroups_;
    return *this;
}

Model::~Model() {}

void Model::addElement(const ModelElement& element) {
    mModelGroups_.push_back(element);
}

void Model::render(VkCommandBuffer cmdBuffer, const void* userPushData, uint32_t userPushSize) {
    for (auto eachElement: mModelGroups_) {
        Mesh* pMesh = eachElement.mesh;
        Material* pMaterial = eachElement.material;

        pMaterial->bindMaterial(cmdBuffer);
        pMesh->bindMesh(cmdBuffer);

        // make a copy of instance data
        std::vector<uint8_t> pushDataCopy(userPushSize);
        std::memcpy(pushDataCopy.data(), userPushData, userPushSize);
        // Assume model is in the beginning
        glm::mat4* modelMat = reinterpret_cast<glm::mat4*>(pushDataCopy.data());
        *modelMat = (*modelMat) * eachElement.localTransform;

        pMaterial->pushConstants(
            cmdBuffer,
            pushDataCopy.data(),
            userPushSize,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        );

        vkCmdDrawIndexed(cmdBuffer, pMesh->getIndicesCount(), 1, 0, 0, 0);
    }
}


} // namespace clay