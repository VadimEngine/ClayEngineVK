#pragma once
// standard lib
#include <queue>
// clay
#include "clay/ecs/Types.h"


namespace clay::ecs {

class EntityManager;
struct Transform;
struct PhysicsBody2D;

class CollisionSystem {
public:
    CollisionSystem() = default;

    void update(EntityManager& entityManager, float dt);

    void checkCollisions(EntityManager& entityManager);

    void resolveCollisions(EntityManager& entityManager);
    
    // 2D AABB collision detection with penetration calculation
    bool checkAABBCollision(
        const glm::vec3& posA, const glm::vec3& scaleA,
        const glm::vec3& posB, const glm::vec3& scaleB,
        glm::vec2& penetration) const;
    
    // 2D collision resolution using MTV (Minimum Translation Vector)
    void resolveCollision2D(
        Transform& transformA, PhysicsBody2D& bodyA,
        Transform& transformB, PhysicsBody2D& bodyB,
        const glm::vec2& penetration);
    
    // Constrain velocities based on predicted collisions (called by PhysicsSystem)
    void constrainVelocitiesForPredictedCollisions(EntityManager& entityManager, float dt);
    
    // Detect and resolve overlapping collisions iteratively (called by PhysicsSystem)
    void resolveOverlappingCollisions(EntityManager& entityManager, int maxIterations = 1);
    
    // TODO set of currently colliding entity pairs (both ways)

private:
    std::queue<CollisionEvent> mCollisionEvents_;

};

} // namespace clay::ecs

