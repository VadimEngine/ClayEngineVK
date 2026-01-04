// class
#include "clay/graphics/common/SkyBox.h"

namespace clay {

SkyBox::SkyBox(clay::Handle<Mesh> meshHandle, clay::Handle<Material> materialHandle)
    : mMeshHandle_(meshHandle), mMaterialHandle_(materialHandle) {}

SkyBox::~SkyBox() {}

void SkyBox::update(glm::quat& cameraOrientation) {
    mModelMat_ = glm::mat4_cast(glm::conjugate(cameraOrientation));
}

} // namespace clay