// clay::Camera's conventions. Games build picking, culling and movement on these, and a
// wrong assumption shows up as mirrored clicks or a camera facing the wrong way, not as an
// error: -Z is forward, +Y is up, and the projection is Vulkan's (Y points down in clip space).
#include <gtest/gtest.h>
// third party
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
// clay
#include "clay/graphics/common/Camera.h"

namespace {

constexpr float kTolerance = 1e-4f;

void expectNear(const glm::vec3& actual, const glm::vec3& expected) {
    EXPECT_NEAR(actual.x, expected.x, kTolerance);
    EXPECT_NEAR(actual.y, expected.y, kTolerance);
    EXPECT_NEAR(actual.z, expected.z, kTolerance);
}

// Where a world point lands in normalised device coordinates.
glm::vec3 toNdc(const clay::Camera& camera, const glm::vec3& world) {
    const glm::vec4 clip = camera.getProjectionMatrix() * camera.getViewMatrix() * glm::vec4(world, 1.0f);
    return glm::vec3(clip) / clip.w;
}

} // namespace

TEST(CameraTest, DefaultFacesNegativeZWithYUp) {
    const clay::Camera camera;
    expectNear(camera.getForward(), {0.0f, 0.0f, -1.0f});
    expectNear(camera.getUp(), {0.0f, 1.0f, 0.0f});
    expectNear(camera.getRight(), {1.0f, 0.0f, 0.0f});
    expectNear(camera.getPosition(), {0.0f, 0.0f, 0.0f});
}

TEST(CameraTest, PointStraightAheadIsAtScreenCentre) {
    clay::Camera camera({2.0f, 3.0f, 4.0f});
    const glm::vec3 ndc = toNdc(camera, camera.getPosition() + camera.getForward() * 10.0f);
    EXPECT_NEAR(ndc.x, 0.0f, kTolerance);
    EXPECT_NEAR(ndc.y, 0.0f, kTolerance);
}

// Vulkan clip space has Y pointing down: something above the camera's line of sight has a
// NEGATIVE ndc y (it is nearer the top of the screen, where pixel y is small). Screen <->
// world code that assumes OpenGL's Y-up mirrors every click about the screen's centre.
TEST(CameraTest, ProjectionIsYDownInBothModes) {
    for (const clay::Camera::Mode mode : {clay::Camera::Mode::PERSPECTIVE, clay::Camera::Mode::ORTHOGRAPHIC}) {
        clay::Camera camera;
        camera.setMode(mode);
        EXPECT_LT(toNdc(camera, {0.0f, 1.0f, -5.0f}).y, 0.0f) << "a point above the view axis";
        EXPECT_GT(toNdc(camera, {0.0f, -1.0f, -5.0f}).y, 0.0f) << "a point below the view axis";
        // X is not flipped: right is positive.
        EXPECT_GT(toNdc(camera, {1.0f, 0.0f, -5.0f}).x, 0.0f);
    }
}

// The orthographic projection is built for Vulkan's 0..1 depth: its near plane (0.1) maps
// to depth 0 and its far plane (100) to depth 1.
TEST(CameraTest, OrthographicDepthRunsFromZeroToOne) {
    clay::Camera camera;
    camera.setMode(clay::Camera::Mode::ORTHOGRAPHIC);
    EXPECT_NEAR(toNdc(camera, {0.0f, 0.0f, -0.1f}).z, 0.0f, kTolerance);
    EXPECT_NEAR(toNdc(camera, {0.0f, 0.0f, -100.0f}).z, 1.0f, kTolerance);
    const float middle = toNdc(camera, {0.0f, 0.0f, -50.0f}).z;
    EXPECT_GT(middle, 0.0f);
    EXPECT_LT(middle, 1.0f);
}

TEST(CameraTest, WiderAspectRatioShrinksXOnly) {
    clay::Camera camera;
    const glm::vec3 point(1.0f, 1.0f, -5.0f);
    camera.setAspectRatio(1.0f);
    const glm::vec3 square = toNdc(camera, point);
    camera.setAspectRatio(2.0f);
    const glm::vec3 wide = toNdc(camera, point);
    EXPECT_NEAR(wide.x, square.x * 0.5f, kTolerance);
    EXPECT_NEAR(wide.y, square.y, kTolerance);
}

TEST(CameraTest, NarrowerFovMagnifies) {
    clay::Camera camera;
    const glm::vec3 point(1.0f, 0.0f, -5.0f);
    camera.setFOV(60.0f);
    const float wide = toNdc(camera, point).x;
    camera.setFOV(30.0f);
    const float narrow = toNdc(camera, point).x;
    EXPECT_GT(narrow, wide);
}

// The documented way to aim the camera: glm::quatLookAt uses the same -Z-forward convention.
TEST(CameraTest, QuatLookAtAimsTheCameraAtATarget) {
    clay::Camera camera({1.0f, 2.0f, 3.0f});
    const glm::vec3 target(-4.0f, 0.5f, -2.0f);
    const glm::vec3 direction = glm::normalize(target - camera.getPosition());
    camera.setOrientation(glm::quatLookAt(direction, glm::vec3(0.0f, 1.0f, 0.0f)));

    expectNear(camera.getForward(), direction);
    const glm::vec3 ndc = toNdc(camera, target);
    EXPECT_NEAR(ndc.x, 0.0f, kTolerance);
    EXPECT_NEAR(ndc.y, 0.0f, kTolerance);
}

TEST(CameraTest, AxesStayPerpendicularUnitVectorsAfterRotating) {
    clay::Camera camera;
    camera.rotate({0.0f, 1.0f, 0.0f}, 37.0f);
    camera.rotate({1.0f, 0.0f, 0.0f}, -21.0f);
    camera.rotate({0.3f, 0.2f, 0.9f}, 64.0f);

    const glm::vec3 f = camera.getForward(), r = camera.getRight(), u = camera.getUp();
    EXPECT_NEAR(glm::length(f), 1.0f, kTolerance);
    EXPECT_NEAR(glm::length(r), 1.0f, kTolerance);
    EXPECT_NEAR(glm::length(u), 1.0f, kTolerance);
    EXPECT_NEAR(glm::dot(f, r), 0.0f, kTolerance);
    EXPECT_NEAR(glm::dot(f, u), 0.0f, kTolerance);
    EXPECT_NEAR(glm::dot(r, u), 0.0f, kTolerance);
    // Right-handed: right x up points backwards (the opposite of forward).
    expectNear(glm::cross(r, u), -f);
}

// rotate() takes degrees and turns counter-clockwise about the axis (right-hand rule).
TEST(CameraTest, RotateTakesDegreesAboutTheAxis) {
    clay::Camera camera;
    camera.rotate({0.0f, 1.0f, 0.0f}, 90.0f);
    expectNear(camera.getForward(), {-1.0f, 0.0f, 0.0f});
    expectNear(camera.getRight(), {0.0f, 0.0f, -1.0f});
}

// move() goes `step` units whatever the length of the direction given.
TEST(CameraTest, MoveTravelsExactlyTheStep) {
    clay::Camera camera({1.0f, 1.0f, 1.0f});
    camera.move({0.0f, 0.0f, -25.0f}, 3.0f);
    expectNear(camera.getPosition(), {1.0f, 1.0f, -2.0f});
    camera.move({3.0f, 4.0f, 0.0f}, 5.0f);
    expectNear(camera.getPosition(), {4.0f, 5.0f, -2.0f});
}

TEST(CameraTest, ZoomAdjustsFovAndStopsAtZero) {
    clay::Camera camera;
    camera.setFOV(45.0f);
    camera.zoom(-10.0f);
    EXPECT_FLOAT_EQ(camera.getFOV(), 35.0f);
    camera.zoom(-100.0f);
    EXPECT_FLOAT_EQ(camera.getFOV(), 0.0f);
}
