#pragma once
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/application/common/Resources.h"


namespace clay::ecs {

class EntityManager;

class RenderSystem {
public:

    RenderSystem(BaseGraphicsContext& gContext, Resources& resources);

    void render(EntityManager& entityManager, VkCommandBuffer cmdBuffer);

    BaseGraphicsContext& mGContext_;
    Resources& mResources_;
};

} // namespace clay::ecs