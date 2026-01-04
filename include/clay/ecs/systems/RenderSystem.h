#pragma once
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/application/common/Resources.h"


namespace clay::ecs {

class EntityManager;

class RenderSystem {
public:

    RenderSystem(BaseGraphicsContext& gContext, Resources& resources);

    void update(EntityManager& entityManager, float dt);

    void render(EntityManager& entityManager, vk::CommandBuffer cmdBuffer);

    void renderModel(const Model& model, vk::CommandBuffer cmdBuffer, const void* userPushData, uint32_t userPushSize);

    // Set debug rendering resources
    void setDebugResources(clay::Handle<Material> materialHandle, clay::Handle<Mesh> meshHandle) {
        mDebugMaterialHandle_ = materialHandle;
        mDebugMeshHandle_ = meshHandle;
    }

    BaseGraphicsContext& mGContext_;
    Resources& mResources_;
    
    bool mDebugDrawColliders_ = false;

private:
    clay::Handle<Material> mDebugMaterialHandle_;
    clay::Handle<Mesh> mDebugMeshHandle_;
};

} // namespace clay::ecs