// The 2D collision maths in clay::ecs::CollisionSystem: detecting two overlapping boxes,
// the shortest push that separates them, and who moves. These two functions take plain
// values, so they are tested here without an EntityManager (which needs a graphics context).
//
// A box is its centre and its half-size ("scale"): it spans centre - scale .. centre + scale.
// The penetration returned is how far box A must move to stop overlapping box B.
#include <gtest/gtest.h>
// standard lib
#include <algorithm>
#include <cmath>
// third party
#include <glm/glm.hpp>
// clay
#include "clay/ecs/Types.h"
#include "clay/ecs/systems/CollisionSystem.h"

namespace {

using clay::ecs::CollisionSystem;
using clay::ecs::PhysicsBody2D;
using clay::ecs::Transform;

const glm::vec3 kUnitBox(1.0f, 1.0f, 1.0f);  // half-size 1: a 2 x 2 box

Transform at(float x, float y) {
    Transform t;
    t.mPosition_ = glm::vec3(x, y, 0.0f);
    return t;
}

PhysicsBody2D body(PhysicsBody2D::Type type, glm::vec2 velocity = {0.0f, 0.0f}) {
    PhysicsBody2D b;
    b.type = type;
    b.velocity = velocity;
    return b;
}

} // namespace

TEST(CollisionTest, SeparatedBoxesDoNotCollide) {
    const CollisionSystem collisions{};
    glm::vec2 penetration(99.0f);
    EXPECT_FALSE(collisions.checkAABBCollision({0, 0, 0}, kUnitBox, {5, 0, 0}, kUnitBox, penetration));
    EXPECT_FALSE(collisions.checkAABBCollision({0, 0, 0}, kUnitBox, {0, -5, 0}, kUnitBox, penetration));
    // Overlapping on one axis only is not a collision.
    EXPECT_FALSE(collisions.checkAABBCollision({0, 0, 0}, kUnitBox, {0.5f, 5, 0}, kUnitBox, penetration));
}

// The push is along the axis that needs the smaller move, and points away from B.
TEST(CollisionTest, PenetrationIsTheShortestWayOut) {
    const CollisionSystem collisions{};
    glm::vec2 penetration(0.0f);

    // B is to the right, overlapping by 0.5: A goes left.
    ASSERT_TRUE(collisions.checkAABBCollision({0, 0, 0}, kUnitBox, {1.5f, 0, 0}, kUnitBox, penetration));
    EXPECT_FLOAT_EQ(penetration.x, -0.5f);
    EXPECT_FLOAT_EQ(penetration.y, 0.0f);

    // B is to the left: A goes right.
    ASSERT_TRUE(collisions.checkAABBCollision({0, 0, 0}, kUnitBox, {-1.5f, 0, 0}, kUnitBox, penetration));
    EXPECT_FLOAT_EQ(penetration.x, 0.5f);
    EXPECT_FLOAT_EQ(penetration.y, 0.0f);

    // B is below (A standing in a floor): A goes up.
    ASSERT_TRUE(collisions.checkAABBCollision({0, 1.75f, 0}, kUnitBox, {0, 0, 0}, kUnitBox, penetration));
    EXPECT_FLOAT_EQ(penetration.x, 0.0f);
    EXPECT_FLOAT_EQ(penetration.y, 0.25f);

    // B is above (A's head in a ceiling): A goes down.
    ASSERT_TRUE(collisions.checkAABBCollision({0, -1.75f, 0}, kUnitBox, {0, 0, 0}, kUnitBox, penetration));
    EXPECT_FLOAT_EQ(penetration.x, 0.0f);
    EXPECT_FLOAT_EQ(penetration.y, -0.25f);
}

// Moving A by the penetration leaves the boxes just touching, for boxes of any size.
TEST(CollisionTest, MovingByThePenetrationSeparatesTheBoxes) {
    const CollisionSystem collisions{};
    const glm::vec3 scaleA(0.5f, 2.0f, 1.0f), scaleB(3.0f, 0.25f, 1.0f);
    const glm::vec3 posB(0.0f, 0.0f, 0.0f);
    for (const glm::vec3 posA : {glm::vec3(3.2f, 0.4f, 0), glm::vec3(-3.3f, -1.0f, 0), glm::vec3(1.0f, 2.1f, 0),
                                 glm::vec3(-2.0f, -2.2f, 0)}) {
        glm::vec2 penetration(0.0f);
        ASSERT_TRUE(collisions.checkAABBCollision(posA, scaleA, posB, scaleB, penetration));
        const glm::vec3 moved = posA + glm::vec3(penetration, 0.0f);
        const float gapX = std::abs(moved.x - posB.x) - (scaleA.x + scaleB.x);
        const float gapY = std::abs(moved.y - posB.y) - (scaleA.y + scaleB.y);
        // Apart (or exactly touching) on at least one axis.
        EXPECT_GE(std::max(gapX, gapY), -1e-5f) << "from (" << posA.x << ", " << posA.y << ")";
    }
}

// Boxes whose edges exactly meet count as colliding, with nothing to push: resting on a
// floor keeps reporting contact without the box being moved.
TEST(CollisionTest, TouchingEdgesCollideWithZeroPenetration) {
    const CollisionSystem collisions{};
    glm::vec2 penetration(99.0f);
    ASSERT_TRUE(collisions.checkAABBCollision({0, 2, 0}, kUnitBox, {0, 0, 0}, kUnitBox, penetration));
    EXPECT_FLOAT_EQ(penetration.x, 0.0f);
    EXPECT_FLOAT_EQ(penetration.y, 0.0f);
}

TEST(CollisionTest, DynamicBodyIsPushedOutOfAStaticOne) {
    CollisionSystem collisions;
    Transform player = at(0.0f, 1.75f), floor = at(0.0f, 0.0f);
    PhysicsBody2D playerBody = body(PhysicsBody2D::Type::DYNAMIC, {3.0f, -8.0f});
    PhysicsBody2D floorBody = body(PhysicsBody2D::Type::STATIC);

    collisions.resolveCollision2D(player, playerBody, floor, floorBody, {0.0f, 0.25f});

    EXPECT_FLOAT_EQ(player.mPosition_.y, 2.0f);
    EXPECT_FLOAT_EQ(floor.mPosition_.y, 0.0f) << "a static body never moves";
    // Landing stops the fall but not the run.
    EXPECT_FLOAT_EQ(playerBody.velocity.y, 0.0f);
    EXPECT_FLOAT_EQ(playerBody.velocity.x, 3.0f);
}

// The same collision seen from the other side (the static body is A): B is the one moved,
// and it moves against the penetration (which is A's way out).
TEST(CollisionTest, StaticFirstMovesTheOtherBodyTheOppositeWay) {
    CollisionSystem collisions;
    Transform wall = at(0.0f, 0.0f), player = at(1.5f, 0.0f);
    PhysicsBody2D wallBody = body(PhysicsBody2D::Type::STATIC);
    PhysicsBody2D playerBody = body(PhysicsBody2D::Type::DYNAMIC, {-4.0f, 2.0f});

    collisions.resolveCollision2D(wall, wallBody, player, playerBody, {-0.5f, 0.0f});

    EXPECT_FLOAT_EQ(wall.mPosition_.x, 0.0f);
    EXPECT_FLOAT_EQ(player.mPosition_.x, 2.0f);
    // Hitting a wall stops sideways motion but not the jump.
    EXPECT_FLOAT_EQ(playerBody.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(playerBody.velocity.y, 2.0f);
}

TEST(CollisionTest, TwoDynamicBodiesShareThePush) {
    CollisionSystem collisions;
    Transform a = at(0.0f, 0.0f), b = at(1.5f, 0.0f);
    PhysicsBody2D bodyA = body(PhysicsBody2D::Type::DYNAMIC, {2.0f, 1.0f});
    PhysicsBody2D bodyB = body(PhysicsBody2D::Type::DYNAMIC, {-2.0f, 1.0f});

    collisions.resolveCollision2D(a, bodyA, b, bodyB, {-0.5f, 0.0f});

    EXPECT_FLOAT_EQ(a.mPosition_.x, -0.25f);
    EXPECT_FLOAT_EQ(b.mPosition_.x, 1.75f);
    EXPECT_FLOAT_EQ(bodyA.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(bodyB.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(bodyA.velocity.y, 1.0f);
    EXPECT_FLOAT_EQ(bodyB.velocity.y, 1.0f);
}

// Two bodies that do not block (pickups, triggers) pass through each other.
TEST(CollisionTest, NonSolidBodiesAreNotMoved) {
    CollisionSystem collisions;
    Transform a = at(0.0f, 0.0f), b = at(1.5f, 0.0f);
    PhysicsBody2D bodyA = body(PhysicsBody2D::Type::DYNAMIC, {2.0f, 0.0f});
    PhysicsBody2D bodyB = body(PhysicsBody2D::Type::DYNAMIC, {-2.0f, 0.0f});
    bodyA.isSolid = false;
    bodyB.isSolid = false;

    collisions.resolveCollision2D(a, bodyA, b, bodyB, {-0.5f, 0.0f});

    EXPECT_FLOAT_EQ(a.mPosition_.x, 0.0f);
    EXPECT_FLOAT_EQ(b.mPosition_.x, 1.5f);
    EXPECT_FLOAT_EQ(bodyA.velocity.x, 2.0f);
    EXPECT_FLOAT_EQ(bodyB.velocity.x, -2.0f);
}
