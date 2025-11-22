#pragma once

namespace clay::ecs {

class EntityManager;

class PhysicsSystem {
public:

    PhysicsSystem();

    void update(EntityManager& entityManager, float dt);
};


} // namespace clay::ecs