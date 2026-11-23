// class
#include "clay/graphics/common/SkyBox.h"

namespace clay {

SkyBox::SkyBox(clay::Handle<Mesh> meshHandle, clay::Handle<Material> materialHandle)
    : mMeshHandle_(meshHandle), mMaterialHandle_(materialHandle) {}

SkyBox::~SkyBox() {}

void SkyBox::update(glm::quat& cameraOrientation) {
    // Post-multiply the extra rotation: the camera-cancel makes the sky world-fixed,
    // so the net sky transform becomes exactly mRotation_ (a fixed-world-axis spin).
    mModelMat_ = glm::mat4_cast(glm::conjugate(cameraOrientation)) * mRotation_;
}

} // namespace clay