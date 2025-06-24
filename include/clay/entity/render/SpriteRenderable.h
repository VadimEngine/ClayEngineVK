#pragma once
// clay
#include "clay/entity/render/BaseRenderable.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/graphics/common/Material.h"

namespace clay {
// for now can just take in texture and x y w h offsets. But maybe later can make a sprite/spritesheet object
// that can also be used for batch rendering by allow easy grouping into ssbo

class SpriteRenderable : public BaseRenderable {
public:
    SpriteRenderable(Mesh* pMesh, Material* pMaterial, glm::vec4 spriteOffset);

    ~SpriteRenderable();

    void render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat) override;

    void setColor(const glm::vec4 newColor);
    

    Mesh* mpMesh_;
    Material* mpMaterial_;
    glm::vec4 mSpriteOffset_;
    glm::vec4 mColor_ = {1,1,1,1};
};

}// namespace clay