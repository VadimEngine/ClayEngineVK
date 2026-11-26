#pragma once
// standard lib
#include <cstdint>
#include <vector>
// third party
#include <glm/glm.hpp>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/Frustum.h"
#include "clay/graphics/common/InstanceData.h"
#include "clay/application/common/Resources.h"
#include "clay/ecs/Types.h"

namespace clay::ecs {

class EntityManager;

class RenderSystem {
public:

    RenderSystem(BaseGraphicsContext& gContext, Resources& resources);
    ~RenderSystem();
    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    void update(EntityManager& entityManager, float dt);

    // Draw every renderable entity. Layers draw in ascending renderLayer order and,
    // within a layer, in entity order — except that consecutive opaque depth-tested
    // MODEL draws are sorted by pipeline/material/mesh so redundant state binds are
    // skipped (they are order-independent, so the image is unchanged).
    void render(EntityManager& entityManager, vk::CommandBuffer cmdBuffer);

    void renderModel(const Model& model, vk::CommandBuffer cmdBuffer, const void* userPushData, uint32_t userPushSize);

    // Draw all static shadow casters (MODEL + TRANSFORM, renderLayer 0 only — so
    // skybox/outlines/UI are skipped) into the currently-bound depth pass, using
    // the given depth-only material. Pushes lightViewProj * model as a single mat4.
    // Casters wholly outside the light's volume are culled.
    void renderShadowCasters(EntityManager& entityManager, vk::CommandBuffer cmdBuffer,
                             Material& shadowMaterial, const glm::mat4& lightViewProj);

    // Frustum culling for render(): MODEL entities whose bounds fall wholly outside
    // this view-projection are skipped. Off by default (everything draws); a scene
    // that renders the world from one camera should set it each frame before render().
    void setCullingViewProj(const glm::mat4& viewProj) {
        mCullFrustums_[0] = Frustum::fromViewProj(viewProj);
        mCullFrustumCount_ = 1;
        mCullEnabled_ = true;
    }
    // Stereo (XR: both eyes drawn in one multiview pass): an entity is skipped only when
    // it is outside BOTH eyes' view-projections.
    void setCullingViewProj(const glm::mat4& leftViewProj, const glm::mat4& rightViewProj) {
        mCullFrustums_[0] = Frustum::fromViewProj(leftViewProj);
        mCullFrustums_[1] = Frustum::fromViewProj(rightViewProj);
        mCullFrustumCount_ = 2;
        mCullEnabled_ = true;
    }
    void disableCulling() { mCullEnabled_ = false; }

    // GPU instancing happens automatically for materials that registered an instanced
    // variant (Material::setInstancedVariant) on platforms that track frames
    // (BaseGraphicsContext::beginFrame). This switch forces the one-draw-per-object
    // path instead — for A/B comparison; CLAY_NO_INSTANCING=1 sets it at start-up.
    void setInstancingEnabled(bool enabled) { mInstancingEnabled_ = enabled; }
    bool isInstancingEnabled() const { return mInstancingEnabled_; }

    // Set debug rendering resources
    void setDebugResources(clay::Handle<Material> materialHandle, clay::Handle<Mesh> meshHandle) {
        mDebugMaterialHandle_ = materialHandle;
        mDebugMeshHandle_ = meshHandle;
    }

    // Global multiplicative tint applied to the push-constant color of 3D world
    // renderables (MODEL + skeletal-animated). Defaults to white, so scenes that
    // never set it look unchanged. Used e.g. to darken the world at night.
    void setWorldTint(const glm::vec3& tint) { mWorldTint_ = tint; }
    const glm::vec3& getWorldTint() const { return mWorldTint_; }

    BaseGraphicsContext& mGContext_;
    Resources& mResources_;

    bool mDebugDrawColliders_ = false;

private:
    // Cached world matrix + world bounding sphere of a MODEL entity. Recomputed only
    // when the entity's transform / local matrix / model changes, and shared by the
    // shadow and main passes, so static scenery costs a compare per frame.
    struct WorldCache {
        Transform transform;
        glm::mat4 localModelMat{1.0f};
        clay::Handle<Model> modelHandle;
        glm::mat4 world{1.0f};
        glm::vec4 sphere{0.0f, 0.0f, 0.0f, -1.0f}; // xyz centre, w radius (<0 = never cull)
        bool valid = false;
    };
    const WorldCache& worldOf(EntityManager& entityManager, Entity e);

    // Model-space bounding sphere of a whole Model (all elements), cached per handle.
    struct ModelBounds {
        uint32_t gen = 0;
        size_t elementCount = 0;
        glm::vec4 sphere{0.0f, 0.0f, 0.0f, -1.0f};
        bool valid = false;
    };
    glm::vec4 modelBounds(clay::Handle<Model> handle);

    // One draw in the frame's sorted list.
    struct DrawItem {
        int layer = 0;
        bool sortable = false;   // opaque depth-tested MODEL element (may be reordered)
        bool isModel = false;    // false: draw the entity through drawOther()
        Entity entity = 0;
        const PipelineResource* pipeline = nullptr;
        const Material* material = nullptr;
        const Material* instanced = nullptr;   // instanced variant to draw with, if any
        Mesh* mesh = nullptr;
        const Model::ModelElement* element = nullptr;
    };
    struct ShadowItem {
        Mesh* mesh = nullptr;
        Entity entity = 0;
        const Model::ModelElement* element = nullptr;
    };

    // Text / sprite / 2D-animation / skinned entities (drawn one by one, in order).
    void drawOther(EntityManager& entityManager, Entity e, vk::CommandBuffer cmdBuffer);
    void drawDebugColliders(EntityManager& entityManager, vk::CommandBuffer cmdBuffer);

    clay::Handle<Material> mDebugMaterialHandle_;
    clay::Handle<Mesh> mDebugMeshHandle_;
    glm::vec3 mWorldTint_ = glm::vec3(1.0f);

    Frustum mCullFrustums_[2]{};
    uint32_t mCullFrustumCount_ = 0;
    bool mCullEnabled_ = false;

    std::vector<WorldCache> mWorldCache_;   // indexed by entity id
    std::vector<ModelBounds> mModelBounds_; // indexed by model handle index
    std::vector<DrawItem> mDrawList_;       // reused every frame (no per-frame allocation)
    std::vector<ShadowItem> mShadowList_;

    // ---- GPU instancing ----
    // One host-visible, persistently mapped vertex buffer of InstanceData per frame
    // slot. Every pass of a frame appends to it; it is rewound when its slot's next
    // frame begins (by which time the GPU is done with it).
    struct InstanceBuffer {
        vk::Buffer buffer{};
        vk::DeviceMemory memory{};
        InstanceData* mapped = nullptr;
        uint32_t capacity = 0;   // in instances
        uint32_t used = 0;
        // Buffers outgrown mid-frame: still referenced by draws already recorded this
        // frame, so they are destroyed when the slot begins its next frame.
        std::vector<std::pair<vk::Buffer, vk::DeviceMemory>> retired;
    };
    // True when instanced draws may be recorded this frame. Also rewinds the frame
    // slot's buffer the first time it is called in a new frame.
    bool instancingActive();
    // Reserve `count` consecutive instances in this frame's buffer. Returns the write
    // pointer and the index of the first one (for drawIndexed's firstInstance).
    InstanceData* reserveInstances(uint32_t count, uint32_t& firstInstance);
    InstanceBuffer& frameInstanceBuffer();
    void destroyInstanceBuffer(InstanceBuffer& buffer);

    std::vector<InstanceBuffer> mInstanceBuffers_;   // indexed by frame slot
    uint64_t mSeenFrameSerial_ = 0;
    bool mInstancingEnabled_ = true;
};

} // namespace clay::ecs
