// standard lib
#include <algorithm>
#include <vector>
#include <cmath>
// clay
#include "clay/ecs/EntityManager.h"
#include "clay/utils/common/Logger.h"
#include "clay/ecs/systems/CollisionSystem.h"
// class
#include "clay/ecs/systems/PhysicsSystem.h"


namespace clay::ecs {

    physx::PxDefaultAllocator PhysicsSystem::sAllocator_;
    physx::PxDefaultErrorCallback PhysicsSystem::sErrorCallback_;

    PhysicsSystem::PhysicsSystem() {
        // Initialize PhysX Foundation
        mPxFoundation_ = PxCreateFoundation(
            PX_PHYSICS_VERSION,
            sAllocator_,
            sErrorCallback_
        );

        if (!mPxFoundation_) {
            LOG_E("PhysX Foundation creation failed");
            return;
        }

        // Initialize PhysX Physics
        mPxPhysics_ = PxCreatePhysics(
            PX_PHYSICS_VERSION,
            *mPxFoundation_,
            physx::PxTolerancesScale()
        );

        if (!mPxPhysics_) {
            LOG_E("PhysX Physics creation failed");
            mPxFoundation_->release();
            mPxFoundation_ = nullptr;
            return;
        }

        LOG_I("PhysicsSystem initialized PhysX successfully");
    }

    PhysicsSystem::~PhysicsSystem() {
        // Release in reverse order of creation
        if (mPxPhysics_) {
            mPxPhysics_->release();
            mPxPhysics_ = nullptr;
        }

        if (mPxFoundation_) {
            mPxFoundation_->release();
            mPxFoundation_ = nullptr;
        }

        LOG_I("PhysicsSystem destroyed");
    }

    physx::PxScene* PhysicsSystem::createScene(const physx::PxSceneDesc& sceneDesc) {
        if (!mPxPhysics_) {
            LOG_E("Cannot create scene: PhysX not initialized");
            return nullptr;
        }

        physx::PxScene* scene = mPxPhysics_->createScene(sceneDesc);
        if (!scene) {
            LOG_E("PhysX Scene creation failed");
        }

        return scene;
    }

    void PhysicsSystem::update(EntityManager& entityManager, float dt) {
        // Update 2D physics
        update2DPhysics(entityManager, dt);
        // Update 3D physics
        update3DPhysics(entityManager, dt);
    }

    void PhysicsSystem::update3DPhysics(EntityManager& entityManager, float dt) {
        // Step the PhysX simulation if a scene is set (3D)
        if (mPxScene_) {
            mPxScene_->simulate(dt);
            mPxScene_->fetchResults(true);
        }

        // Sync PhysX transforms
        for (clay::ecs::Entity e: entityManager.mCurrentEntities_) {
            if (entityManager.mSignatures[e][clay::ecs::ComponentType::METADATA] && !entityManager.mMetaData[e].enabled) {
                continue;
            }

            if (entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && entityManager.mSignatures[e][clay::ecs::ComponentType::PHYSX_RIGID_BODY]) {
                // sync transform and rigid body
                const physx::PxTransform t = entityManager.mPhysXRigidBodies[e].actor->getGlobalPose();
                entityManager.mTransforms[e].mPosition_ = {
                    t.p.x, t.p.y, t.p.z
                };
                entityManager.mTransforms[e].mOrientation_ = {
                    t.q.w, t.q.x, t.q.y, t.q.z
                };
            }
        }
    }

    void PhysicsSystem::update2DPhysics(EntityManager& entityManager, float dt) {
        // First pass: Apply gravity to velocities
        for (clay::ecs::Entity e: entityManager.mCurrentEntities_) {
            if (entityManager.mSignatures[e][clay::ecs::ComponentType::METADATA] && !entityManager.mMetaData[e].enabled) {
                continue;
            }

            if (!entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] || 
                !entityManager.mSignatures[e][clay::ecs::ComponentType::PHYSICS_BODY_2D]) {
                continue;
            }

            auto& body = entityManager.mPhysicsBodies2D[e];

            if (body.type == PhysicsBody2D::Type::STATIC) {
                continue;
            }

            // Apply gravity
            if (body.type == PhysicsBody2D::Type::DYNAMIC) {
                body.velocity += mGravity2D_ * dt;
                
                // Clamp velocity
                const float maxVelocity = 10.0f;
                body.velocity.x = std::clamp(body.velocity.x, -maxVelocity, maxVelocity);
                body.velocity.y = std::clamp(body.velocity.y, -maxVelocity, maxVelocity);
            }
        }

        // Second pass: Check if movement would cause collision and constrain velocity
        // Use CollisionSystem for this
        CollisionSystem collisionSystem;
        collisionSystem.constrainVelocitiesForPredictedCollisions(entityManager, dt);

        // Third pass: Move entities with constrained velocities
        for (clay::ecs::Entity e: entityManager.mCurrentEntities_) {
            if (entityManager.mSignatures[e][clay::ecs::ComponentType::METADATA] && !entityManager.mMetaData[e].enabled) {
                continue;
            }

            if (!entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] || 
                !entityManager.mSignatures[e][clay::ecs::ComponentType::PHYSICS_BODY_2D]) {
                continue;
            }

            auto& transform = entityManager.mTransforms[e];
            auto& body = entityManager.mPhysicsBodies2D[e];

            if (body.type == PhysicsBody2D::Type::STATIC) {
                continue;
            }

            // Move
            transform.mPosition_.x += body.velocity.x * dt;
            transform.mPosition_.y += body.velocity.y * dt;
        }

        // Fourth pass: Resolve any remaining collisions until no overlaps remain
        collisionSystem.resolveOverlappingCollisions(entityManager, 1);
    }


} // namespace clay::ecs