#pragma once
// standard lib
#include <stdint.h>
#include <bitset>
#include <array>
#include <queue>
// third party
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
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
    COLLIDER,
    RIGID_BODY,
    PARENT,
    METADATA, 
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

    glm::vec3 offset = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
};

struct Parent {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::PARENT);

    Entity entity;
};

struct ModelRenderable {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::MODEL);

    Resources::Handle<Model> modelHandle;
    glm::vec4 mColor_ = {1,1,1,1};
    glm::mat4 localModelMat = glm::identity<glm::mat4>();
};

struct SpriteRenderable {
    Mesh* mpMesh_;
    Material* mpMaterial_;
    glm::vec4 mSpriteOffset_;
    glm::vec4 mColor_ = {1,1,1,1};
};

struct EntityMeta {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::TEXT);

    bool enabled;
};

struct RigidBody {
    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::RIGID_BODY);

    glm::vec3 velocity = {0.0f, 0.0f, 0.0f};
    float mMaxSpeed_ = 100.f;
    float mass = 1.0f;
    float gravityScale = 0.f;
    bool attractive = false;
    
};

} // clay::ecs