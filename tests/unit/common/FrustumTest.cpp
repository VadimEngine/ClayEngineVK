// Frustum culling must never reject something that is visible. These tests compare
// clay::Frustum against a brute-force clip-space test for the two projections the
// engine culls with: a perspective camera and an orthographic (shadow) light.
#include <gtest/gtest.h>
// standard lib
#include <cmath>
#include <random>
// third party
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
// clay
#include "clay/graphics/common/Frustum.h"

namespace {

// Vulkan clip space: -w <= x,y <= w and 0 <= z <= w.
bool insideClip(const glm::mat4& viewProj, const glm::vec3& p) {
    const glm::vec4 c = viewProj * glm::vec4(p, 1.0f);
    return c.x >= -c.w && c.x <= c.w && c.y >= -c.w && c.y <= c.w && c.z >= 0.0f && c.z <= c.w;
}

// Distance from p to the nearest frustum plane (to skip points float noise can flip).
float nearestPlane(const clay::Frustum& f, const glm::vec3& p) {
    float nearest = 1e30f;
    for (const glm::vec4& plane : f.planes) {
        nearest = std::min(nearest, std::fabs(glm::dot(glm::vec3(plane), p) + plane.w));
    }
    return nearest;
}

void expectMatchesClipTest(const glm::mat4& viewProj, float range) {
    const clay::Frustum frustum = clay::Frustum::fromViewProj(viewProj);
    std::mt19937 rng(42u);
    std::uniform_real_distribution<float> coord(-range, range);

    int inside = 0;
    for (int i = 0; i < 100000; ++i) {
        const glm::vec3 p(coord(rng), coord(rng), coord(rng));
        if (nearestPlane(frustum, p) < 1e-3f) continue;

        const bool expected = insideClip(viewProj, p);
        EXPECT_EQ(frustum.intersectsSphere(p, 0.0f), expected);
        if (expected) {
            ++inside;
            // A sphere containing a visible point must be reported visible.
            EXPECT_TRUE(frustum.intersectsSphere(p + glm::vec3(0.3f, -0.2f, 0.1f), 0.5f));
        }
    }
    EXPECT_GT(inside, 100) << "test volume never intersected the frustum";
}

} // namespace

TEST(FrustumTest, PerspectiveCameraMatchesClipSpace) {
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
    proj[1][1] *= -1.0f; // Vulkan Y-down, as clay::Camera produces
    const glm::mat4 view = glm::lookAt(glm::vec3(3, 4, 5), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
    expectMatchesClipTest(proj * view, 60.0f);
}

TEST(FrustumTest, OrthographicLightMatchesClipSpace) {
    const glm::vec3 sunDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.5f));
    const glm::mat4 view = glm::lookAt(sunDir * 45.0f, glm::vec3(0.0f), glm::vec3(0, 1, 0));
    const glm::mat4 proj = glm::orthoRH_ZO(-32.0f, 32.0f, -32.0f, 32.0f, 1.0f, 140.0f);
    expectMatchesClipTest(proj * view, 80.0f);
}

TEST(FrustumTest, SphereStraddlingAPlaneIsVisible) {
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 1.0f, 100.0f);
    proj[1][1] *= -1.0f;
    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    const clay::Frustum frustum = clay::Frustum::fromViewProj(proj * view);

    // Centre is behind the camera, but the sphere reaches past the near plane.
    EXPECT_TRUE(frustum.intersectsSphere(glm::vec3(0, 0, 1.0f), 3.0f));
    // Wholly behind the camera.
    EXPECT_FALSE(frustum.intersectsSphere(glm::vec3(0, 0, 10.0f), 3.0f));
    // Wholly beyond the far plane.
    EXPECT_FALSE(frustum.intersectsSphere(glm::vec3(0, 0, -200.0f), 3.0f));
}
