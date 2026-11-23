#pragma once
// third party
#include <glm/glm.hpp>

namespace clay {

// The six clip planes of a view-projection matrix, for visibility tests. Built for
// Vulkan clip space (depth 0..1); works for perspective and orthographic projections
// (so it also describes a directional light's shadow volume) and is unaffected by
// the Y flip in the projection.
struct Frustum {
    // Each plane is (normal.xyz, d) with the normal pointing INTO the frustum, so a
    // point p is inside when dot(normal, p) + d >= 0. Normals are unit length.
    glm::vec4 planes[6];

    static Frustum fromViewProj(const glm::mat4& vp) {
        // Gribb/Hartmann plane extraction. glm is column-major: row i = (vp[0][i], ...).
        const glm::vec4 r0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
        const glm::vec4 r1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
        const glm::vec4 r2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
        const glm::vec4 r3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

        Frustum f;
        f.planes[0] = r3 + r0; // left
        f.planes[1] = r3 - r0; // right
        f.planes[2] = r3 + r1; // bottom / top (whichever the Y flip makes it)
        f.planes[3] = r3 - r1;
        f.planes[4] = r2;      // near (clip z >= 0)
        f.planes[5] = r3 - r2; // far  (clip z <= w)
        for (glm::vec4& p : f.planes) {
            const float len = glm::length(glm::vec3(p));
            if (len > 0.0f) p /= len;
        }
        return f;
    }

    // True when the sphere is at least partly inside (conservative: may return true
    // for a sphere just outside a frustum corner, never false for a visible one).
    bool intersectsSphere(const glm::vec3& centre, float radius) const {
        for (const glm::vec4& p : planes) {
            if (p.x * centre.x + p.y * centre.y + p.z * centre.z + p.w < -radius) return false;
        }
        return true;
    }
};

} // namespace clay
