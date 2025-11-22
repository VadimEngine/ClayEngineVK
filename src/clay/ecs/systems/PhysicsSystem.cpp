// clay
#include "clay/ecs/EntityManager.h"
#include "clay/utils/common/Logger.h"
// class
#include "clay/ecs/systems/PhysicsSystem.h"


namespace clay::ecs {

    PhysicsSystem::PhysicsSystem() {}

    void PhysicsSystem::update(EntityManager& entityManager, float dt) {
        for (clay::ecs::Entity e: entityManager.mCurrentEntities_) {
            if (entityManager.mSignatures[e][clay::ecs::ComponentType::METADATA] && !entityManager.mMetaData[e].enabled) {
                continue;
            }

            if (entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && entityManager.mSignatures[e][clay::ecs::ComponentType::RIGID_BODY]) {
                // sync transform and rigid body
                const physx::PxTransform t = entityManager.mRigidBodies[e].actor->getGlobalPose();
                entityManager.mTransforms[e].mPosition_ = {
                    t.p.x, t.p.y, t.p.z
                };
                entityManager.mTransforms[e].mOrientation_ = {
                    t.q.w, t.q.x, t.q.y, t.q.z
                };
            } else {
            }
        }
    }


} // namespace clay::ecs