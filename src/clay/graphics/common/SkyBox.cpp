// class
#include "clay/graphics/common/SkyBox.h"

namespace clay {

SkyBox::SkyBox(Mesh& mesh, Material& material)
    : mMesh_(mesh), mMaterial_(material) {}

SkyBox::~SkyBox() {}

void SkyBox::update(glm::quat& cameraOrientation, float dt) {
    mModelMat_ = glm::mat4_cast(glm::conjugate(cameraOrientation));
}

void SkyBox::render(vk::CommandBuffer cmdBuffer) {
    mMaterial_.bindMaterial(cmdBuffer);
    mMesh_.bindMesh(cmdBuffer);

    struct PushConstants {
        glm::mat4 model;
        glm::vec4 color;
    } push{};

    push.model = mModelMat_;
    push.color = {1,1,1,1};

    mMaterial_.pushConstants(
        cmdBuffer,
        &push,
        sizeof(glm::mat4),
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
    );

    cmdBuffer.drawIndexed(mMesh_.getIndicesCount(), 1, 0, 0, 0);
}

} // namespace clay