#pragma once
// third party
#include <PxPhysicsAPI.h>
// clay
#include "clay/ecs/Types.h"

namespace clay::ecs {

class EntityManager;

class PhysicsSystem {
public:

    PhysicsSystem();
    ~PhysicsSystem();

    // Delete copy/move constructors
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    PhysicsSystem(PhysicsSystem&&) = delete;
    PhysicsSystem& operator=(PhysicsSystem&&) = delete;

    void update(EntityManager& entityManager, float dt);

    // PhysX management (3D)
    void update3DPhysics(EntityManager& entityManager, float dt);
    physx::PxPhysics* getPhysics() { return mPxPhysics_; }
    physx::PxFoundation* getFoundation() { return mPxFoundation_; }
    physx::PxScene* createScene(const physx::PxSceneDesc& sceneDesc);
    void setScene(physx::PxScene* scene) { mPxScene_ = scene; }
    bool isInitialized() const { return mPxPhysics_ != nullptr; }

    // 2D physics management
    void setGravity2D(const glm::vec2& gravity) { mGravity2D_ = gravity; }
    glm::vec2 getGravity2D() const { return mGravity2D_; }

private:
    // PhysX (3D)
    physx::PxFoundation* mPxFoundation_ = nullptr;
    physx::PxPhysics* mPxPhysics_ = nullptr;
    physx::PxScene* mPxScene_ = nullptr;

    // Static allocator and error callback (must outlive PhysX objects)
    static physx::PxDefaultAllocator sAllocator_;
    static physx::PxDefaultErrorCallback sErrorCallback_;

    // 2D physics
    glm::vec2 mGravity2D_ = glm::vec2(0.0f, -20.0f); // Default gravity

    // 2D physics methods
    void update2DPhysics(EntityManager& entityManager, float dt);
};


} // namespace clay::ecs