// standard lib
#include <algorithm>
#include <vector>
#include <cmath>
// clay
#include "clay/ecs/EntityManager.h"
#include "clay/utils/common/Logger.h"
// class
#include "clay/ecs/systems/CollisionSystem.h"

namespace clay::ecs {

void CollisionSystem::update(EntityManager& entityManager, float dt) {
    checkCollisions(entityManager);
    resolveCollisions(entityManager);
}

void CollisionSystem::checkCollisions(EntityManager& entityManager) {
    for (clay::ecs::Entity e: entityManager.mCurrentEntities_) {
        if (entityManager.mSignatures[e][clay::ecs::ComponentType::METADATA] && !entityManager.mMetaData[e].enabled) {
            continue;
        }

        if (entityManager.mSignatures[e][clay::ecs::ComponentType::COLLIDER] && entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM]) {
            // check collisions with other entities
            for (clay::ecs::Entity other: entityManager.mCurrentEntities_) {
                if (e == other) continue;

                if (entityManager.mSignatures[other][clay::ecs::ComponentType::COLLIDER] && entityManager.mSignatures[other][clay::ecs::ComponentType::TRANSFORM]) {
                    // simple AABB collision detection for example
                    const auto& transformA = entityManager.mTransforms[e];
                    const auto& colliderA = entityManager.mColliders[e];
                    const auto& transformB = entityManager.mTransforms[other];
                    const auto& colliderB = entityManager.mColliders[other];

                    bool isColliding = false;

                    if (colliderA.mType_ == Collider::Type::AABB && colliderB.mType_ == Collider::Type::AABB) {
                        glm::vec3 posA = transformA.mPosition_ + colliderA.offset;
                        glm::vec3 posB = transformB.mPosition_ + colliderB.offset;

                        if (std::abs(posA.x - posB.x) <= (colliderA.aabb.halfExtents.x + colliderB.aabb.halfExtents.x) &&
                            std::abs(posA.y - posB.y) <= (colliderA.aabb.halfExtents.y + colliderB.aabb.halfExtents.y) &&
                            std::abs(posA.z - posB.z) <= (colliderA.aabb.halfExtents.z + colliderB.aabb.halfExtents.z)) {
                            isColliding = true;
                        }
                    } else if (colliderA.mType_ == Collider::Type::CIRCLE && colliderB.mType_ == Collider::Type::CIRCLE) {
                        glm::vec3 posA = transformA.mPosition_ + colliderA.offset;
                        glm::vec3 posB = transformB.mPosition_ + colliderB.offset;

                        glm::vec3 diff = posA - posB;
                        float distanceSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                        float radiusSum = colliderA.circle.radius + colliderB.circle.radius;

                        if (distanceSq <= radiusSum * radiusSum) {
                            isColliding = true;
                        }
                    } else if ((colliderA.mType_ == Collider::Type::AABB && colliderB.mType_ == Collider::Type::CIRCLE) ||
                               (colliderA.mType_ == Collider::Type::CIRCLE && colliderB.mType_ == Collider::Type::AABB)) {
                        const Collider& aabbCollider = (colliderA.mType_ == Collider::Type::AABB) ? colliderA : colliderB;
                        const Transform& aabbTransform = (colliderA.mType_ == Collider::Type::AABB) ? transformA : transformB;
                        const Collider& circleCollider = (colliderA.mType_ == Collider::Type::CIRCLE) ? colliderA : colliderB;
                        const Transform& circleTransform = (colliderA.mType_ == Collider::Type::CIRCLE) ? transformA : transformB;

                        glm::vec3 aabbPos = aabbTransform.mPosition_ + aabbCollider.offset;
                        glm::vec3 circlePos = circleTransform.mPosition_ + circleCollider.offset;

                        // Find the closest point on the AABB to the circle center
                        glm::vec3 closestPoint;
                        closestPoint.x = std::max(aabbPos.x - aabbCollider.aabb.halfExtents.x,
                                            std::min(circlePos.x, aabbPos.x + aabbCollider.aabb.halfExtents.x));
                        closestPoint.y = std::max(aabbPos.y - aabbCollider.aabb.halfExtents.y,
                                            std::min(circlePos.y, aabbPos.y + aabbCollider.aabb.halfExtents.y));
                        closestPoint.z = std::max(aabbPos.z - aabbCollider.aabb.halfExtents.z,
                                            std::min(circlePos.z, aabbPos.z + aabbCollider.aabb.halfExtents.z));

                        glm::vec3 diff = closestPoint - circlePos;
                        float distanceSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

                        if (distanceSq <= circleCollider.circle.radius * circleCollider.circle.radius) {
                            isColliding = true;
                        }
                    }

                    if (isColliding) {
                        // Only create collision events if at least one entity has COLLISION_ACTION
                        bool hasCollisionAction = entityManager.mSignatures[e][clay::ecs::ComponentType::COLLISION_ACTION] ||
                                                  entityManager.mSignatures[other][clay::ecs::ComponentType::COLLISION_ACTION];
                        
                        if (hasCollisionAction) {
                            CollisionEvent event;
                            event.type = CollisionEvent::Type::ENTER;
                            event.a = e;
                            event.b = other;
                            mCollisionEvents_.push(event);
                        }
                    }
                }
            }
        }
    }

}

void CollisionSystem::resolveCollisions(EntityManager& entityManager) {
    int count = 0;
    while (!mCollisionEvents_.empty()) {
        CollisionEvent event = mCollisionEvents_.front();
        mCollisionEvents_.pop();

        // Check if either entity has PhysicsBody2D for solid collision response
        bool hasPhysicsA = entityManager.mSignatures[event.a][ComponentType::PHYSICS_BODY_2D];
        bool hasPhysicsB = entityManager.mSignatures[event.b][ComponentType::PHYSICS_BODY_2D];
        
        if (hasPhysicsA && hasPhysicsB) {
            auto& physicsA = entityManager.mPhysicsBodies2D[event.a];
            auto& physicsB = entityManager.mPhysicsBodies2D[event.b];
            
            // If both are solid and at least one is dynamic, resolve overlap
            if (physicsA.isSolid && physicsB.isSolid && !physicsA.isTrigger && !physicsB.isTrigger) {
                bool aDynamic = physicsA.type == PhysicsBody2D::Type::DYNAMIC;
                bool bDynamic = physicsB.type == PhysicsBody2D::Type::DYNAMIC;
                
                if (aDynamic || bDynamic) {
                    auto& transformA = entityManager.mTransforms[event.a];
                    auto& transformB = entityManager.mTransforms[event.b];
                    auto& colliderA = entityManager.mColliders[event.a];
                    auto& colliderB = entityManager.mColliders[event.b];
                    
                    // Calculate overlap and push entities apart
                    glm::vec3 posA = transformA.mPosition_ + colliderA.offset;
                    glm::vec3 posB = transformB.mPosition_ + colliderB.offset;
                    glm::vec3 delta = posA - posB;
                    
                    if (colliderA.mType_ == Collider::Type::AABB && colliderB.mType_ == Collider::Type::AABB) {
                        // AABB vs AABB separation
                        glm::vec3 overlap;
                        overlap.x = (colliderA.aabb.halfExtents.x + colliderB.aabb.halfExtents.x) - std::abs(delta.x);
                        overlap.y = (colliderA.aabb.halfExtents.y + colliderB.aabb.halfExtents.y) - std::abs(delta.y);
                        
                        // Find minimum overlap axis (only separate on that axis)
                        if (overlap.x < overlap.y) {
                            float sign = (delta.x > 0) ? 1.0f : -1.0f;
                            if (aDynamic && !bDynamic) {
                                transformA.mPosition_.x += overlap.x * sign;
                                physicsA.velocity.x = 0.0f; // Stop movement on collision axis
                            } else if (!aDynamic && bDynamic) {
                                transformB.mPosition_.x -= overlap.x * sign;
                                physicsB.velocity.x = 0.0f; // Stop movement on collision axis
                            } else if (aDynamic && bDynamic) {
                                transformA.mPosition_.x += overlap.x * 0.5f * sign;
                                transformB.mPosition_.x -= overlap.x * 0.5f * sign;
                                physicsA.velocity.x = 0.0f; // Stop movement on collision axis
                                physicsB.velocity.x = 0.0f; // Stop movement on collision axis
                            }
                        } else {
                            float sign = (delta.y > 0) ? 1.0f : -1.0f;
                            if (aDynamic && !bDynamic) {
                                transformA.mPosition_.y += overlap.y * sign;
                                physicsA.velocity.y = 0.0f; // Stop movement on collision axis
                            } else if (!aDynamic && bDynamic) {
                                transformB.mPosition_.y -= overlap.y * sign;
                                physicsB.velocity.y = 0.0f; // Stop movement on collision axis
                            } else if (aDynamic && bDynamic) {
                                transformA.mPosition_.y += overlap.y * 0.5f * sign;
                                transformB.mPosition_.y -= overlap.y * 0.5f * sign;
                                physicsA.velocity.y = 0.0f; // Stop movement on collision axis
                                physicsB.velocity.y = 0.0f; // Stop movement on collision axis
                            }
                        }
                    }
                }
            }
        }

        // Call collision action callbacks
        if (entityManager.mSignatures[event.a][clay::ecs::ComponentType::COLLISION_ACTION]) {
            const auto& collisionActionA = entityManager.mCollisionActions[event.a];
            if (event.type == CollisionEvent::Type::ENTER && collisionActionA.onEnter) {
                collisionActionA.onEnter(event.b);
            } else if (event.type == CollisionEvent::Type::EXIT && collisionActionA.onExit) {
                collisionActionA.onExit(event.b);
            }
        }

        if (entityManager.mSignatures[event.b][clay::ecs::ComponentType::COLLISION_ACTION]) {
            const auto& collisionActionB = entityManager.mCollisionActions[event.b];
            if (event.type == CollisionEvent::Type::ENTER && collisionActionB.onEnter) {
                collisionActionB.onEnter(event.a);
            } else if (event.type == CollisionEvent::Type::EXIT && collisionActionB.onExit) {
                collisionActionB.onExit(event.a);
            }
        }
    }
}

bool CollisionSystem::checkAABBCollision(
    const glm::vec3& posA, const glm::vec3& scaleA,
    const glm::vec3& posB, const glm::vec3& scaleB,
    glm::vec2& penetration) const {
    
    // Calculate min and max bounds for both AABBs
    glm::vec2 minA = glm::vec2(posA.x - scaleA.x, posA.y - scaleA.y);
    glm::vec2 maxA = glm::vec2(posA.x + scaleA.x, posA.y + scaleA.y);
    glm::vec2 minB = glm::vec2(posB.x - scaleB.x, posB.y - scaleB.y);
    glm::vec2 maxB = glm::vec2(posB.x + scaleB.x, posB.y + scaleB.y);

    // Check for overlap
    bool overlapX = maxA.x >= minB.x && maxB.x >= minA.x;
    bool overlapY = maxA.y >= minB.y && maxB.y >= minA.y;

    if (!overlapX || !overlapY) {
        return false;
    }

    // Calculate separation needed to resolve overlap
    // Positive penetration = A needs to move in positive direction to separate from B
    float separationLeft = maxA.x - minB.x;   // A overlaps B from left, push A left (negative)
    float separationRight = maxB.x - minA.x;  // A overlaps B from right, push A right (positive)
    float separationBottom = maxA.y - minB.y; // A overlaps B from bottom, push A down (negative)
    float separationTop = maxB.y - minA.y;    // A overlaps B from top, push A up (positive)

    // Choose minimum separation on each axis
    float separationX = (separationLeft < separationRight) ? -separationLeft : separationRight;
    float separationY = (separationBottom < separationTop) ? -separationBottom : separationTop;

    // Use the axis with minimum penetration (MTV)
    if (std::abs(separationX) < std::abs(separationY)) {
        penetration.x = separationX;
        penetration.y = 0.0f;
    } else {
        penetration.x = 0.0f;
        penetration.y = separationY;
    }

    return true;
}

void CollisionSystem::resolveCollision2D(
    Transform& transformA, PhysicsBody2D& bodyA,
    Transform& transformB, PhysicsBody2D& bodyB,
    const glm::vec2& penetration) {
    
    bool aIsStatic = bodyA.type == PhysicsBody2D::Type::STATIC;
    bool bIsStatic = bodyB.type == PhysicsBody2D::Type::STATIC;

    if (!bodyA.isSolid && !bodyB.isSolid) {
        return;
    }

    // Use pure MTV (Minimum Translation Vector) for separation
    // penetration is already the MTV from collision detection
    
    // Determine if this is a vertical or horizontal collision
    bool isVertical = std::abs(penetration.y) > std::abs(penetration.x);

    // Separate objects using MTV
    if (aIsStatic && !bIsStatic) {
        // B is dynamic, move it away from static A by MTV
        transformB.mPosition_.x -= penetration.x;
        transformB.mPosition_.y -= penetration.y;
        
        // Zero velocity in collision direction
        if (isVertical) {
            bodyB.velocity.y = 0.0f;
        } else {
            bodyB.velocity.x = 0.0f;
        }
    } else if (bIsStatic && !aIsStatic) {
        // A is dynamic, move it away from static B by MTV
        transformA.mPosition_.x += penetration.x;
        transformA.mPosition_.y += penetration.y;
        
        // Zero velocity in collision direction
        if (isVertical) {
            bodyA.velocity.y = 0.0f;
        } else {
            bodyA.velocity.x = 0.0f;
        }
    } else {
        // Both dynamic - split MTV equally
        transformA.mPosition_.x += penetration.x * 0.5f;
        transformA.mPosition_.y += penetration.y * 0.5f;
        transformB.mPosition_.x -= penetration.x * 0.5f;
        transformB.mPosition_.y -= penetration.y * 0.5f;
        
        // Zero velocities
        if (isVertical) {
            bodyA.velocity.y = 0.0f;
            bodyB.velocity.y = 0.0f;
        } else {
            bodyA.velocity.x = 0.0f;
            bodyB.velocity.x = 0.0f;
        }
    }
}

void CollisionSystem::constrainVelocitiesForPredictedCollisions(EntityManager& entityManager, float dt) {
    std::vector<Entity> physicsEntities;
    for (Entity e : entityManager.mCurrentEntities_) {
        if (entityManager.mSignatures[e][ComponentType::TRANSFORM] &&
            entityManager.mSignatures[e][ComponentType::PHYSICS_BODY_2D] &&
            entityManager.mSignatures[e][ComponentType::COLLIDER]) {
            physicsEntities.push_back(e);
        }
    }

    // For each dynamic entity, check if its intended movement would cause collision
    for (Entity e : physicsEntities) {
        auto& transform = entityManager.mTransforms[e];
        auto& body = entityManager.mPhysicsBodies2D[e];
        auto& collider = entityManager.mColliders[e];

        if (body.type != PhysicsBody2D::Type::DYNAMIC || !body.isSolid) {
            continue;
        }

        // Check for collisions with other entities
        for (Entity other : physicsEntities) {
            if (e == other) continue;

            auto& otherTransform = entityManager.mTransforms[other];
            auto& otherBody = entityManager.mPhysicsBodies2D[other];
            auto& otherCollider = entityManager.mColliders[other];

            if (!otherBody.isSolid) continue;

            if (collider.mType_ == Collider::Type::AABB && 
                otherCollider.mType_ == Collider::Type::AABB) {
                
                glm::vec3 worldHalfExtentsA = collider.aabb.halfExtents * transform.mScale_;
                glm::vec3 worldHalfExtentsB = otherCollider.aabb.halfExtents * otherTransform.mScale_;
                
                // First check if already overlapping
                glm::vec2 currentPenetration;
                bool currentlyOverlapping = checkAABBCollision(
                    transform.mPosition_ + collider.offset,
                    glm::vec3(worldHalfExtentsA.x, worldHalfExtentsA.y, 0.0f),
                    otherTransform.mPosition_ + otherCollider.offset,
                    glm::vec3(worldHalfExtentsB.x, worldHalfExtentsB.y, 0.0f),
                    currentPenetration);
                
                if (currentlyOverlapping) {
                    // Already overlapping - prevent movement that would increase overlap
                    // Check if velocity is moving us deeper into the collision
                    if (currentPenetration.x > 0 && body.velocity.x > 0) {
                        body.velocity.x = 0.0f;  // Moving right into collision
                    } else if (currentPenetration.x < 0 && body.velocity.x < 0) {
                        body.velocity.x = 0.0f;  // Moving left into collision
                    }
                    
                    if (currentPenetration.y > 0 && body.velocity.y > 0) {
                        body.velocity.y = 0.0f;  // Moving up into collision
                    } else if (currentPenetration.y < 0 && body.velocity.y < 0) {
                        body.velocity.y = 0.0f;  // Moving down into collision
                    }
                } else {
                    // Not currently overlapping - check X and Y axes independently
                    // This prevents horizontal collisions from affecting vertical movement
                    
                    // Check X movement independently
                    if (body.velocity.x != 0.0f) {
                        glm::vec3 intendedPosX = transform.mPosition_;
                        intendedPosX.x += body.velocity.x * dt;
                        // Don't add Y movement here
                        
                        glm::vec2 penetrationX;
                        if (checkAABBCollision(
                            intendedPosX + collider.offset,
                            glm::vec3(worldHalfExtentsA.x, worldHalfExtentsA.y, 0.0f),
                            otherTransform.mPosition_ + otherCollider.offset,
                            glm::vec3(worldHalfExtentsB.x, worldHalfExtentsB.y, 0.0f),
                            penetrationX)) {
                            
                            // Only constrain X if the collision is primarily horizontal
                            if (penetrationX.x != 0.0f) {
                                body.velocity.x = 0.0f;
                            }
                        }
                    }
                    
                    // Check Y movement independently
                    if (body.velocity.y != 0.0f) {
                        glm::vec3 intendedPosY = transform.mPosition_;
                        intendedPosY.y += body.velocity.y * dt;
                        // Don't add X movement here
                        
                        glm::vec2 penetrationY;
                        if (checkAABBCollision(
                            intendedPosY + collider.offset,
                            glm::vec3(worldHalfExtentsA.x, worldHalfExtentsA.y, 0.0f),
                            otherTransform.mPosition_ + otherCollider.offset,
                            glm::vec3(worldHalfExtentsB.x, worldHalfExtentsB.y, 0.0f),
                            penetrationY)) {
                            
                            // Only constrain Y if the collision is primarily vertical
                            if (penetrationY.y != 0.0f) {
                                body.velocity.y = 0.0f;
                            }
                        }
                    }
                }
            }
        }
    }
}

void CollisionSystem::resolveOverlappingCollisions(EntityManager& entityManager, int maxIterations) {
    bool hadCollision = true;
    
    for (int iteration = 0; iteration < maxIterations && hadCollision; ++iteration) {
        hadCollision = false;
        
        std::vector<Entity> physicsEntities;
        for (Entity e : entityManager.mCurrentEntities_) {
            if (entityManager.mSignatures[e][ComponentType::TRANSFORM] &&
                entityManager.mSignatures[e][ComponentType::PHYSICS_BODY_2D] &&
                entityManager.mSignatures[e][ComponentType::COLLIDER]) {
                physicsEntities.push_back(e);
            }
        }

        // Check all pairs and resolve any collision found
        for (size_t i = 0; i < physicsEntities.size(); ++i) {
            for (size_t j = i + 1; j < physicsEntities.size(); ++j) {
                Entity entityA = physicsEntities[i];
                Entity entityB = physicsEntities[j];

                auto& transformA = entityManager.mTransforms[entityA];
                auto& transformB = entityManager.mTransforms[entityB];
                auto& bodyA = entityManager.mPhysicsBodies2D[entityA];
                auto& bodyB = entityManager.mPhysicsBodies2D[entityB];
                auto& colliderA = entityManager.mColliders[entityA];
                auto& colliderB = entityManager.mColliders[entityB];

                if (bodyA.type == PhysicsBody2D::Type::STATIC && 
                    bodyB.type == PhysicsBody2D::Type::STATIC) {
                    continue;
                }

                if (colliderA.mType_ == Collider::Type::AABB && 
                    colliderB.mType_ == Collider::Type::AABB) {
                    
                    glm::vec2 penetration;
                    glm::vec3 worldHalfExtentsA = colliderA.aabb.halfExtents * transformA.mScale_;
                    glm::vec3 worldHalfExtentsB = colliderB.aabb.halfExtents * transformB.mScale_;
                    if (checkAABBCollision(
                        transformA.mPosition_ + colliderA.offset,
                        glm::vec3(worldHalfExtentsA.x, worldHalfExtentsA.y, 0.0f),
                        transformB.mPosition_ + colliderB.offset,
                        glm::vec3(worldHalfExtentsB.x, worldHalfExtentsB.y, 0.0f),
                        penetration)) {
                        
                        hadCollision = true;
                        
                        if (!bodyA.isTrigger && !bodyB.isTrigger) {
                            // Apply MTV to separate overlapping objects
                            resolveCollision2D(transformA, bodyA, transformB, bodyB, penetration);
                            
                            // Break inner loops to restart collision check from beginning
                            i = physicsEntities.size();
                            j = physicsEntities.size();
                        }
                    }
                }
            }
        }
    }
}

} // namespace clay::ecs