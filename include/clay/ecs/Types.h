#pragma once
// standard lib
#include <stdint.h>
#include <bitset>
#include <array>
#include <queue>
// third party
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <PxPhysicsAPI.h>

// clay
#include "clay/application/common/Resources.h"

namespace clay::ecs {

// Used to define the size of arrays later on
using Entity = uint32_t;
static constexpr Entity MAX_ENTITIES = 5000;

enum ComponentType : uint8_t {
    TRANSFORM = 0,
    MODEL, 
    TEXT,
    SPRITE,
    ANIMATION_2D_RENDERABLE,
    SKELETAL_ANIMATION_RENDERABLE,
    COLLIDER,
    PHYSX_RIGID_BODY,
    PHYSICS_BODY_2D,
    PARENT,
    METADATA, 
    COLLISION_ACTION,
    MAX_COMPONENTS 
};
using Signature = std::bitset<MAX_COMPONENTS>;

struct Transform {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::TRANSFORM);

    glm::vec3 mPosition_ = glm::vec3(0, 0 , 0);
    glm::quat mOrientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 mScale_ = glm::vec3(1, 1, 1);
};

// sphere only for now
struct Collider {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::COLLIDER);

    enum class Type {
        AABB,
        CIRCLE,
    } mType_ = Type::AABB;

    glm::vec3 offset = glm::vec3(0.0f, 0.0f, 0.0f);

    union {
        struct {
            glm::vec3 halfExtents;
        } aabb;
        struct {
            float radius;
        } circle;
    };
};

struct Parent {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::PARENT);

    Entity entity;
};

struct ModelRenderable {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::MODEL);

    clay::Handle<Model> modelHandle;
    glm::vec4 mColor_ = {1,1,1,1};
    glm::mat4 localModelMat = glm::identity<glm::mat4>();
    int renderLayer = 0; // Lower values render first: -100=skybox, 0=default, 100=UI
};

struct SpriteRenderable {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::SPRITE);

    clay::Handle<Mesh> meshHandle;
    clay::Handle<Material> materialHandle;
    glm::vec4 mSpriteOffset_;
    glm::vec4 mColor_ = {1,1,1,1};
};

struct Animation2DRenderable {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::ANIMATION_2D_RENDERABLE);
    
    clay::Handle<Animation2D> animationHandle;
    clay::Handle<Mesh> meshHandle;
    glm::vec4 mColor_ = {1,1,1,1};
    glm::vec3 mOffset_ = {0,0,0};
    
    // Playback state (per-entity)
    uint32_t currentFrame = 0;
    float currentTime = 0.0f;
    bool playing = true;
};

struct Animation3DRenderable {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::SKELETAL_ANIMATION_RENDERABLE);
    
    clay::Handle<AnimatedMesh> meshHandle;
    clay::Handle<SkeletalAnimation> animationHandle;
    clay::Handle<Material> materialHandle;
    glm::vec4 mColor_ = {1,1,1,1};
    
    // Bone transforms buffer (shared)
    vk::Buffer boneTransformsBuffer_{};
    vk::DeviceMemory boneTransformsMemory_{};
    
    // Per-entity playback state
    float currentTime = 0.0f;
    bool playing = false;
    bool looping = true;
};

struct EntityMetadata {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::METADATA);

    bool enabled = true;
};

struct PhysicsBody2D {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::PHYSICS_BODY_2D);

    enum class Type {
        STATIC,      // Immovable (walls, trees)
        DYNAMIC,     // Full physics (player, enemies, projectiles)
        KINEMATIC    // Moves but ignores physics (moving platforms)
    };
    
    Type type = Type::DYNAMIC;
    
    // Dynamic properties (ignored for STATIC)
    glm::vec2 velocity = {0.0f, 0.0f};
    float mass = 1.0f;
    
    // Collision properties (all types)
    bool isSolid = true;        // blocks movement
    bool isTrigger = false;     // only fires events, no blocking
    float bounciness = 0.0f;
};

struct PhysXRigidBody {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::PHYSX_RIGID_BODY);

    physx::PxRigidActor* actor;    
};

struct CollisionEvent {

    enum class Type {
        ENTER,
        EXIT
    } type;

    Entity a;
    Entity b;
};

struct CollisionAction {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::COLLISION_ACTION);

    // Called when collision starts
    std::function<void(Entity other)> onEnter;

    // Called when collision ends
    std::function<void(Entity other)> onExit;
};

} // clay::ecs