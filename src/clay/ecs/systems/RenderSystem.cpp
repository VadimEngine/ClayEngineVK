// standard lib
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>
// third party
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
// clay
#include "clay/ecs/EntityManager.h"
#include <clay/utils/common/Logger.h>
#include <clay/utils/common/Profiler.h>
#include "clay/graphics/common/AnimatedMesh.h"
#include "clay/graphics/common/SkeletalAnimation.h"
#include "clay/graphics/common/Model.h"
#include "clay/graphics/common/Tilemap.h"
// class
#include "clay/ecs/systems/RenderSystem.h"

namespace clay::ecs {

namespace {

struct ModelPush {
    glm::mat4 model;
    glm::vec4 color; // optional depending on material
};

// Largest axis scale of a matrix's upper 3x3 (how much it can grow a radius).
float maxAxisScale(const glm::mat4& m) {
    const float sx = glm::dot(glm::vec3(m[0]), glm::vec3(m[0]));
    const float sy = glm::dot(glm::vec3(m[1]), glm::vec3(m[1]));
    const float sz = glm::dot(glm::vec3(m[2]), glm::vec3(m[2]));
    return std::sqrt(std::max(sx, std::max(sy, sz)));
}

// translate * rotate * scale, built directly (no intermediate matrices).
glm::mat4 composeTRS(const Transform& t) {
    glm::mat4 m = glm::mat4_cast(t.mOrientation_);
    m[0] *= t.mScale_.x;
    m[1] *= t.mScale_.y;
    m[2] *= t.mScale_.z;
    m[3] = glm::vec4(t.mPosition_, 1.0f);
    return m;
}

constexpr vk::ShaderStageFlags kPushStages =
    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

} // namespace

RenderSystem::RenderSystem(BaseGraphicsContext& gContext, Resources& resources)
    : mGContext_(gContext), mResources_(resources) {
    // CLAY_NO_INSTANCING=1 forces one draw per object (A/B comparison, debugging).
    if (const char* env = std::getenv("CLAY_NO_INSTANCING"); env && env[0] != '\0' && env[0] != '0') {
        mInstancingEnabled_ = false;
    }
}

RenderSystem::~RenderSystem() {
    // The owner guarantees the device is idle by the time a scene's systems die.
    for (InstanceBuffer& buffer : mInstanceBuffers_) destroyInstanceBuffer(buffer);
}

void RenderSystem::destroyInstanceBuffer(InstanceBuffer& buffer) {
    vk::Device device = mGContext_.getDevice();
    for (auto& [retiredBuffer, retiredMemory] : buffer.retired) {
        device.destroyBuffer(retiredBuffer, nullptr);
        device.freeMemory(retiredMemory, nullptr);   // freeing also unmaps
    }
    buffer.retired.clear();
    if (buffer.buffer) device.destroyBuffer(buffer.buffer, nullptr);
    if (buffer.memory) device.freeMemory(buffer.memory, nullptr);
    buffer = InstanceBuffer{};
}

RenderSystem::InstanceBuffer& RenderSystem::frameInstanceBuffer() {
    const uint32_t slot = mGContext_.getFrameSlot();
    if (mInstanceBuffers_.size() <= slot) mInstanceBuffers_.resize(slot + 1);
    return mInstanceBuffers_[slot];
}

bool RenderSystem::instancingActive() {
    if (!mInstancingEnabled_) return false;
    const uint64_t serial = mGContext_.getFrameSerial();
    if (serial == 0) return false;   // this platform does not track frames

    if (serial != mSeenFrameSerial_) {
        // First pass of a new frame: the slot's previous frame is finished on the GPU,
        // so rewind its buffer and drop any buffers it outgrew.
        mSeenFrameSerial_ = serial;
        InstanceBuffer& buffer = frameInstanceBuffer();
        buffer.used = 0;
        vk::Device device = mGContext_.getDevice();
        for (auto& [retiredBuffer, retiredMemory] : buffer.retired) {
            device.destroyBuffer(retiredBuffer, nullptr);
            device.freeMemory(retiredMemory, nullptr);
        }
        buffer.retired.clear();
    }
    return true;
}

InstanceData* RenderSystem::reserveInstances(uint32_t count, uint32_t& firstInstance) {
    InstanceBuffer& buffer = frameInstanceBuffer();
    if (buffer.used + count > buffer.capacity) {
        // Grow. Draws already recorded this frame still reference the old buffer, so
        // it is only retired here and destroyed when this slot's next frame begins.
        if (buffer.buffer) buffer.retired.emplace_back(buffer.buffer, buffer.memory);
        const uint32_t capacity = std::max<uint32_t>({ 4096u, count * 2u, buffer.capacity * 2u });
        mGContext_.createBuffer(
            sizeof(InstanceData) * capacity,
            vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            buffer.buffer,
            buffer.memory
        );
        buffer.mapped = static_cast<InstanceData*>(
            mGContext_.getDevice().mapMemory(buffer.memory, 0, VK_WHOLE_SIZE));
        buffer.capacity = capacity;
        buffer.used = 0;
    }
    firstInstance = buffer.used;
    buffer.used += count;
    return buffer.mapped + firstInstance;
}

void RenderSystem::update(EntityManager& entityManager, float dt) {
    // Update Animation2DRenderable animations
    for (Entity e : entityManager.mCurrentEntities_) {
        if (!entityManager.mSignatures[e][ComponentType::ANIMATION_2D_RENDERABLE] ||
            !entityManager.mSignatures[e][ComponentType::TRANSFORM]) {
            continue;
        }

        auto& animRenderable = entityManager.mAnimation2DRenderables[e];
        const Animation2D& animation = mResources_[animRenderable.animationHandle];

        if (!animation.frames.empty() && animRenderable.playing) {
            animRenderable.currentTime += dt;

            // Check if we need to advance to the next frame
            if (animRenderable.currentTime >= animation.frameDuration) {
                animRenderable.currentTime -= animation.frameDuration;
                animRenderable.currentFrame++;

                // Handle looping
                if (animRenderable.currentFrame >= animation.frames.size()) {
                    if (animation.loop) {
                        animRenderable.currentFrame = 0;
                    } else {
                        animRenderable.currentFrame = static_cast<uint32_t>(animation.frames.size() - 1);
                        animRenderable.playing = false;
                    }
                }
            }
        } else if (!animation.frames.empty() && !animRenderable.playing) {
            // Not playing - reset to first frame
            animRenderable.currentFrame = 0;
            animRenderable.currentTime = 0.0f;
        }
    }
}

glm::vec4 RenderSystem::modelBounds(clay::Handle<Model> handle) {
    if (mModelBounds_.size() <= handle.index) mModelBounds_.resize(handle.index + 1);
    ModelBounds& bounds = mModelBounds_[handle.index];

    const auto& elements = mResources_[handle].getElements();
    if (bounds.valid && bounds.gen == handle.gen && bounds.elementCount == elements.size()) {
        return bounds.sphere;
    }

    // Union of every element's (transformed) mesh sphere. Unknown mesh bounds make the
    // whole model unknown, which means "never cull".
    bool known = !elements.empty();
    glm::vec3 lo(std::numeric_limits<float>::max());
    glm::vec3 hi(std::numeric_limits<float>::lowest());
    glm::vec4 single(0.0f, 0.0f, 0.0f, -1.0f);
    for (const auto& el : elements) {
        const glm::vec4& meshSphere = mResources_[el.meshHandle].getBoundingSphere();
        if (meshSphere.w < 0.0f) { known = false; break; }
        const glm::vec3 centre = glm::vec3(el.localTransform * glm::vec4(glm::vec3(meshSphere), 1.0f));
        const float radius = meshSphere.w * maxAxisScale(el.localTransform);
        single = glm::vec4(centre, radius);
        lo = glm::min(lo, centre - glm::vec3(radius));
        hi = glm::max(hi, centre + glm::vec3(radius));
    }

    if (!known)                    bounds.sphere = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    else if (elements.size() == 1) bounds.sphere = single;
    else                           bounds.sphere = glm::vec4((lo + hi) * 0.5f, glm::length(hi - lo) * 0.5f);

    bounds.gen = handle.gen;
    bounds.elementCount = elements.size();
    bounds.valid = true;
    return bounds.sphere;
}

const RenderSystem::WorldCache& RenderSystem::worldOf(EntityManager& entityManager, Entity e) {
    if (mWorldCache_.size() < MAX_ENTITIES) mWorldCache_.resize(MAX_ENTITIES);
    WorldCache& cache = mWorldCache_[e];

    const Transform& transform = entityManager.mTransforms[e];
    const ModelRenderable& model = entityManager.mModelRenderable[e];

    if (cache.valid &&
        cache.modelHandle.index == model.modelHandle.index &&
        cache.modelHandle.gen == model.modelHandle.gen &&
        std::memcmp(&cache.transform, &transform, sizeof(Transform)) == 0 &&
        std::memcmp(&cache.localModelMat, &model.localModelMat, sizeof(glm::mat4)) == 0) {
        return cache;
    }

    cache.transform = transform;
    cache.localModelMat = model.localModelMat;
    cache.modelHandle = model.modelHandle;
    cache.world = composeTRS(transform) * model.localModelMat;

    const glm::vec4 local = modelBounds(model.modelHandle);
    if (local.w < 0.0f) {
        cache.sphere = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    } else {
        // A little slack covers vertex-shader displacement (waves, outline hulls, sway).
        const glm::vec3 centre = glm::vec3(cache.world * glm::vec4(glm::vec3(local), 1.0f));
        cache.sphere = glm::vec4(centre, local.w * maxAxisScale(cache.world) * 1.05f + 0.25f);
    }
    cache.valid = true;
    return cache;
}

void RenderSystem::render(EntityManager& entityManager, vk::CommandBuffer cmdBuffer) {
    // Render skybox first if it exists
    if (entityManager.hasSkybox()) {
        SkyBox* skybox = entityManager.getSkybox();
        Material& material = mResources_[skybox->getMaterialHandle()];
        Mesh& mesh = mResources_[skybox->getMeshHandle()];

        material.bindMaterial(cmdBuffer);
        mesh.bindMesh(cmdBuffer);

        ModelPush push{ skybox->getModelMatrix(), skybox->getColor() };
        material.pushConstants(cmdBuffer, &push, sizeof(push), kPushStages);

        cmdBuffer.drawIndexed(mesh.getIndicesCount(), 1, 0, 0, 0);
    }

    // Render tilemaps (background layer)
    for (clay::Tilemap* tilemap : entityManager.getTilemaps()) {
        if (tilemap->getTileCount() > 0) {
            Material& material = mResources_[tilemap->getMaterialHandle()];
            Mesh& mesh = mResources_[tilemap->getMeshHandle()];

            // Bind material (pipeline + descriptors including SSBO)
            material.bindMaterial(cmdBuffer);

            // Bind mesh vertex/index buffers
            mesh.bindMesh(cmdBuffer);

            // Draw all tiles with one instanced draw call
            cmdBuffer.drawIndexed(
                mesh.getIndicesCount(),      // index count per tile
                tilemap->getTileCount(),      // instance count (number of tiles)
                0,                            // first index
                0,                            // vertex offset
                0                             // first instance
            );
        }
    }

    // ---- Gather: one DrawItem per visible model element / other renderable ----
    const bool instancing = instancingActive();
    mDrawList_.clear();
    uint32_t culled = 0;

    for (Entity e : entityManager.mCurrentEntities_) {
        const Signature& sig = entityManager.mSignatures[e];
        if (sig[ComponentType::METADATA] && !entityManager.mMetaData[e].enabled) continue;
        if (!sig[ComponentType::TRANSFORM]) continue;

        if (sig[ComponentType::MODEL]) {
            const ModelRenderable& renderable = entityManager.mModelRenderable[e];
            const WorldCache& cache = worldOf(entityManager, e);
            if (mCullEnabled_ && renderable.mCullable_ && cache.sphere.w >= 0.0f) {
                bool visible = false;
                for (uint32_t f = 0; f < mCullFrustumCount_ && !visible; ++f) {
                    visible = mCullFrustums_[f].intersectsSphere(glm::vec3(cache.sphere), cache.sphere.w);
                }
                if (!visible) {
                    ++culled;
                    continue;
                }
            }
            for (const auto& element : mResources_[renderable.modelHandle].getElements()) {
                Material& material = mResources_[element.materialHandle];
                DrawItem item;
                item.layer = renderable.renderLayer;
                item.isModel = true;
                item.sortable = material.getPipelineResource().isDepthTestedAndWritten();
                // Order-independent draws on a material with an instanced variant are
                // batched: every object sharing its mesh goes out in one draw call.
                if (instancing && item.sortable && material.hasInstancedVariant()) {
                    item.instanced = &mResources_[material.getInstancedVariant()];
                }
                item.entity = e;
                item.pipeline = &material.getPipelineResource();
                item.material = &material;
                item.mesh = &mResources_[element.meshHandle];
                item.element = &element;
                mDrawList_.push_back(item);
            }
        } else if (sig[ComponentType::TEXT] || sig[ComponentType::SPRITE] ||
                   sig[ComponentType::ANIMATION_2D_RENDERABLE] ||
                   sig[ComponentType::SKELETAL_ANIMATION_RENDERABLE]) {
            DrawItem item;
            // Skinned meshes carry their own layer; the rest live on the default one.
            item.layer = sig[ComponentType::SKELETAL_ANIMATION_RENDERABLE]
                       ? entityManager.mAnimation3DRenderables[e].renderLayer : 0;
            item.entity = e;
            mDrawList_.push_back(item);
        }
    }

    // ---- Order: layers ascending, entity order within a layer (stable) ----
    std::stable_sort(mDrawList_.begin(), mDrawList_.end(),
                     [](const DrawItem& a, const DrawItem& b) { return a.layer < b.layer; });

    // Within each run of consecutive order-independent draws, group by pipeline, then
    // material, then mesh, so the emit loop below skips almost every state bind.
    for (size_t i = 0; i < mDrawList_.size();) {
        if (!mDrawList_[i].sortable) { ++i; continue; }
        size_t j = i + 1;
        while (j < mDrawList_.size() && mDrawList_[j].sortable && mDrawList_[j].layer == mDrawList_[i].layer) ++j;
        if (j - i > 2) {
            std::sort(mDrawList_.begin() + i, mDrawList_.begin() + j,
                      [](const DrawItem& a, const DrawItem& b) {
                          if (a.pipeline != b.pipeline) return std::less<const PipelineResource*>()(a.pipeline, b.pipeline);
                          if (a.material != b.material) return std::less<const Material*>()(a.material, b.material);
                          if (a.mesh != b.mesh)         return std::less<const Mesh*>()(a.mesh, b.mesh);
                          return a.entity < b.entity;
                      });
        }
        i = j;
    }

    // ---- Emit, binding a material / mesh only when it changes ----
    const Material* boundMaterial = nullptr;
    const Mesh* boundMesh = nullptr;
    uint32_t drawn = 0;

    // Instanced draws read their model matrix + colour from this frame's instance
    // buffer: reserve room for all of them up front and bind it once.
    uint32_t instanceTotal = 0;
    for (const DrawItem& item : mDrawList_) if (item.instanced) ++instanceTotal;
    uint32_t instanceBase = 0;
    InstanceData* instances = nullptr;
    if (instanceTotal > 0) {
        instances = reserveInstances(instanceTotal, instanceBase);
        const vk::Buffer instanceBuffer = frameInstanceBuffer().buffer;
        const vk::DeviceSize offset = 0;
        cmdBuffer.bindVertexBuffers(InstanceData::kBinding, 1, &instanceBuffer, &offset);
    }
    uint32_t written = 0;

    const auto tintedColor = [this](const ModelRenderable& renderable) {
        if (renderable.mIgnoreWorldTint_) return renderable.mColor_;   // ghost/overlay: raw colour
        return glm::vec4(glm::vec3(renderable.mColor_) * mWorldTint_, renderable.mColor_.a);
    };

    for (size_t i = 0; i < mDrawList_.size();) {
        const DrawItem& item = mDrawList_[i];
        if (!item.isModel) {
            drawOther(entityManager, item.entity, cmdBuffer);
            boundMaterial = nullptr; // drawOther binds its own state
            boundMesh = nullptr;
            ++drawn;
            ++i;
            continue;
        }

        if (item.instanced) {
            // One draw for the whole run of objects sharing this material + mesh.
            const uint32_t first = instanceBase + written;
            size_t j = i;
            for (; j < mDrawList_.size(); ++j) {
                const DrawItem& member = mDrawList_[j];
                if (member.instanced != item.instanced || member.mesh != item.mesh) break;
                InstanceData& data = instances[written++];
                data.model = mWorldCache_[member.entity].world * member.element->localTransform;
                data.color = tintedColor(entityManager.mModelRenderable[member.entity]);
            }
            if (item.instanced != boundMaterial) {
                item.instanced->bindMaterial(cmdBuffer);
                boundMaterial = item.instanced;
            }
            if (item.mesh != boundMesh) {
                item.mesh->bindMesh(cmdBuffer);
                boundMesh = item.mesh;
            }
            cmdBuffer.drawIndexed(item.mesh->getIndicesCount(), static_cast<uint32_t>(j - i), 0, 0, first);
            ++drawn;
            i = j;
            continue;
        }
        ++i;

        if (item.material != boundMaterial) {
            item.material->bindMaterial(cmdBuffer);
            boundMaterial = item.material;
        }
        if (item.mesh != boundMesh) {
            item.mesh->bindMesh(cmdBuffer);
            boundMesh = item.mesh;
        }

        ModelPush push;
        push.model = mWorldCache_[item.entity].world * item.element->localTransform;
        push.color = tintedColor(entityManager.mModelRenderable[item.entity]);

        item.material->pushConstants(cmdBuffer, &push, sizeof(push), kPushStages);
        cmdBuffer.drawIndexed(item.mesh->getIndicesCount(),
                              entityManager.mModelRenderable[item.entity].instanceCount, 0, 0, 0);
        ++drawn;
    }

    CLAY_PROFILE_COUNT("Draws (main)", drawn);
    CLAY_PROFILE_COUNT("Instanced (main)", instanceTotal);
    CLAY_PROFILE_COUNT("Culled (main)", culled);

    if (mDebugDrawColliders_) drawDebugColliders(entityManager, cmdBuffer);
}

void RenderSystem::drawOther(EntityManager& entityManager, Entity e, vk::CommandBuffer cmdBuffer) {
    const Transform& transform = entityManager.mTransforms[e];
    const glm::mat4 world = composeTRS(transform);

    if (entityManager.mSignatures[e][ComponentType::TEXT]) {
        TextRenderable& text = entityManager.mTextRenderables[e];

        text.mpFont_->getMaterial().bindMaterial(cmdBuffer);

        ModelPush push{ world * glm::scale(glm::mat4(1.0f), text.mScale_), text.mColor_ };
        text.mpFont_->getMaterial().pushConstants(cmdBuffer, &push, sizeof(push), kPushStages);

        vk::Buffer vertexBuffers[] = { text.mVertexBuffer_ };
        vk::DeviceSize offsets[] = { 0 };

        cmdBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
        cmdBuffer.draw(static_cast<uint32_t>(text.mVertices_.size()), 1, 0, 0);

    } else if (entityManager.mSignatures[e][ComponentType::SPRITE]) {
        SpriteRenderable& sprite = entityManager.mSpriteRenderables[e];

        Material& material = mResources_[sprite.materialHandle];
        Mesh& mesh = mResources_[sprite.meshHandle];

        material.bindMaterial(cmdBuffer);

        struct PushConstants {
            glm::mat4 model;
            glm::vec4 color;
            glm::vec4 offsets;
        } push{ world, sprite.mColor_, sprite.mSpriteOffset_ };

        material.pushConstants(cmdBuffer, &push, sizeof(push), kPushStages);

        mesh.bindMesh(cmdBuffer);
        cmdBuffer.drawIndexed(mesh.getIndicesCount(), 1, 0, 0, 0);

    } else if (entityManager.mSignatures[e][ComponentType::ANIMATION_2D_RENDERABLE]) {
        Animation2DRenderable& animRenderable = entityManager.mAnimation2DRenderables[e];
        const Animation2D& animation = mResources_[animRenderable.animationHandle];

        // Bind the animation's material (spritesheet)
        Material& material = mResources_[animation.materialHandle];
        material.bindMaterial(cmdBuffer);

        Transform offsetTransform = transform;
        offsetTransform.mPosition_ += animRenderable.mOffset_;

        struct PushConstants {
            glm::mat4 model;
            glm::vec4 color;
            glm::vec4 offsets;
        } push{
            composeTRS(offsetTransform),
            animRenderable.mColor_,
            // Use current frame from animation
            !animation.frames.empty() ? animation.frames[animRenderable.currentFrame] : glm::vec4(0)
        };

        material.pushConstants(cmdBuffer, &push, sizeof(push), kPushStages);

        Mesh& mesh = mResources_[animRenderable.meshHandle];
        mesh.bindMesh(cmdBuffer);
        cmdBuffer.drawIndexed(mesh.getIndicesCount(), 1, 0, 0, 0);

    } else if (entityManager.mSignatures[e][ComponentType::SKELETAL_ANIMATION_RENDERABLE]) {
        Animation3DRenderable& animRenderable = entityManager.mAnimation3DRenderables[e];
        if (!animRenderable.visible) return; // app-side culling (see Animation3DRenderable)

        AnimatedMesh& mesh = mResources_[animRenderable.meshHandle];
        Material& material = mResources_[animRenderable.materialHandle];

        material.bindMaterial(cmdBuffer);

        ModelPush push;
        push.model = world;
        push.color = glm::vec4(glm::vec3(animRenderable.mColor_) * mWorldTint_, animRenderable.mColor_.a);

        material.pushConstants(cmdBuffer, &push, sizeof(push), kPushStages);

        mesh.bindMesh(cmdBuffer);
        cmdBuffer.drawIndexed(mesh.getIndicesCount(), animRenderable.instanceCount, 0, 0, 0);
    }
}

void RenderSystem::drawDebugColliders(EntityManager& entityManager, vk::CommandBuffer cmdBuffer) {
    Material& debugMaterial = mResources_[mDebugMaterialHandle_];
    Mesh& planeMesh = mResources_[mDebugMeshHandle_];

    debugMaterial.bindMaterial(cmdBuffer);
    planeMesh.bindMesh(cmdBuffer);

    // One unit quad, stretched to `scale` at `pos`.
    auto drawQuad = [&](const glm::vec3& pos, const glm::vec3& scale, const glm::vec4& color) {
        ModelPush push{
            glm::translate(glm::mat4(1.0f), pos) * glm::scale(glm::mat4(1.0f), scale),
            color
        };
        debugMaterial.pushConstants(cmdBuffer, &push, sizeof(push), kPushStages);
        cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
    };
    // A box outline as four thin edge quads (top, bottom, left, right).
    auto drawBoxOutline = [&](const glm::vec3& centre, const glm::vec3& halfExtents, const glm::vec4& color) {
        const float lineThickness = 0.02f;
        drawQuad(centre + glm::vec3(0, halfExtents.y, 0),  glm::vec3(halfExtents.x * 2.0f, lineThickness, 1.0f), color);
        drawQuad(centre + glm::vec3(0, -halfExtents.y, 0), glm::vec3(halfExtents.x * 2.0f, lineThickness, 1.0f), color);
        drawQuad(centre + glm::vec3(-halfExtents.x, 0, 0), glm::vec3(lineThickness, halfExtents.y * 2.0f, 1.0f), color);
        drawQuad(centre + glm::vec3(halfExtents.x, 0, 0),  glm::vec3(lineThickness, halfExtents.y * 2.0f, 1.0f), color);
    };

    const glm::vec4 kGreen(0.0f, 1.0f, 0.0f, 1.0f);
    const glm::vec4 kGreenFill(0.0f, 1.0f, 0.0f, 0.3f);
    const glm::vec4 kMagenta(1.0f, 0.0f, 1.0f, 1.0f);     // PhysX shapes
    const glm::vec4 kMagentaFill(1.0f, 0.0f, 1.0f, 0.3f);

    // ECS colliders
    for (Entity e : entityManager.mCurrentEntities_) {
        if (!entityManager.mSignatures[e][ComponentType::COLLIDER] ||
            !entityManager.mSignatures[e][ComponentType::TRANSFORM]) {
            continue;
        }

        const auto& transform = entityManager.mTransforms[e];
        const auto& collider = entityManager.mColliders[e];
        const glm::vec3 colliderPos = transform.mPosition_ + collider.offset;

        if (collider.mType_ == Collider::Type::AABB) {
            // Apply scale to match collision detection world space
            drawBoxOutline(colliderPos, collider.aabb.halfExtents * transform.mScale_, kGreen);
        } else if (collider.mType_ == Collider::Type::CIRCLE) {
            const float d = collider.circle.radius * 2.0f;
            drawQuad(colliderPos, glm::vec3(d, d, 1.0f), kGreenFill);
        }
    }

    // PhysX rigid body shapes
    std::vector<physx::PxShape*> shapes;
    for (Entity e : entityManager.mCurrentEntities_) {
        if (!entityManager.mSignatures[e][ComponentType::PHYSX_RIGID_BODY] ||
            !entityManager.mSignatures[e][ComponentType::TRANSFORM]) {
            continue;
        }

        const auto& rigidBody = entityManager.mPhysXRigidBodies[e];
        const auto& transform = entityManager.mTransforms[e];
        if (!rigidBody.actor) continue;

        const physx::PxU32 numShapes = rigidBody.actor->getNbShapes();
        if (numShapes == 0) continue;
        shapes.resize(numShapes);
        rigidBody.actor->getShapes(shapes.data(), numShapes);

        for (physx::PxShape* shape : shapes) {
            physx::PxGeometryHolder geomHolder = shape->getGeometry();
            const physx::PxTransform localPose = shape->getLocalPose();
            const glm::vec3 colliderPos =
                transform.mPosition_ + glm::vec3(localPose.p.x, localPose.p.y, localPose.p.z);

            if (geomHolder.getType() == physx::PxGeometryType::eBOX) {
                const physx::PxBoxGeometry& box = geomHolder.box();
                drawBoxOutline(colliderPos,
                               glm::vec3(box.halfExtents.x, box.halfExtents.y, box.halfExtents.z), kMagenta);
            } else if (geomHolder.getType() == physx::PxGeometryType::eSPHERE) {
                const float d = geomHolder.sphere().radius * 2.0f;
                drawQuad(colliderPos, glm::vec3(d, d, 1.0f), kMagentaFill);
            }
        }
    }
}

void RenderSystem::renderShadowCasters(EntityManager& entityManager, vk::CommandBuffer cmdBuffer,
                                       Material& shadowMaterial, const glm::mat4& lightViewProj) {
    // Only casters inside the light's volume can affect the shadow map.
    const Frustum lightVolume = Frustum::fromViewProj(lightViewProj);

    mShadowList_.clear();
    uint32_t culled = 0;
    for (Entity e : entityManager.mCurrentEntities_) {
        const Signature& sig = entityManager.mSignatures[e];
        if (sig[ComponentType::METADATA] && !entityManager.mMetaData[e].enabled) continue;
        if (!(sig[ComponentType::TRANSFORM] && sig[ComponentType::MODEL])) continue;

        const ModelRenderable& renderable = entityManager.mModelRenderable[e];
        if (renderable.renderLayer != 0) continue; // world only (skip skybox/outlines/UI)

        const WorldCache& cache = worldOf(entityManager, e);
        if (cache.sphere.w >= 0.0f &&
            !lightVolume.intersectsSphere(glm::vec3(cache.sphere), cache.sphere.w)) {
            ++culled;
            continue;
        }
        for (const auto& element : mResources_[renderable.modelHandle].getElements()) {
            mShadowList_.push_back({ &mResources_[element.meshHandle], e, &element });
        }
    }

    // One material for the whole pass, so sorting by mesh leaves push + draw per caster.
    std::sort(mShadowList_.begin(), mShadowList_.end(),
              [](const ShadowItem& a, const ShadowItem& b) {
                  if (a.mesh != b.mesh) return std::less<const Mesh*>()(a.mesh, b.mesh);
                  return a.entity < b.entity;
              });

    uint32_t drawn = 0;
    if (instancingActive() && shadowMaterial.hasInstancedVariant() && !mShadowList_.empty()) {
        // Instanced: the light matrix is pushed once, each caster's model matrix comes
        // from the instance buffer, and every caster sharing a mesh is one draw.
        const Material& instanced = mResources_[shadowMaterial.getInstancedVariant()];
        instanced.bindMaterial(cmdBuffer);
        instanced.pushConstants(cmdBuffer, &lightViewProj, sizeof(lightViewProj), kPushStages);

        uint32_t instanceBase = 0;
        InstanceData* instances = reserveInstances(static_cast<uint32_t>(mShadowList_.size()), instanceBase);
        const vk::Buffer instanceBuffer = frameInstanceBuffer().buffer;
        const vk::DeviceSize offset = 0;
        cmdBuffer.bindVertexBuffers(InstanceData::kBinding, 1, &instanceBuffer, &offset);

        for (size_t i = 0; i < mShadowList_.size();) {
            const ShadowItem& item = mShadowList_[i];
            size_t j = i;
            for (; j < mShadowList_.size() && mShadowList_[j].mesh == item.mesh; ++j) {
                const ShadowItem& member = mShadowList_[j];
                instances[j].model = mWorldCache_[member.entity].world * member.element->localTransform;
                instances[j].color = glm::vec4(1.0f);
            }
            item.mesh->bindMesh(cmdBuffer);
            cmdBuffer.drawIndexed(item.mesh->getIndicesCount(), static_cast<uint32_t>(j - i), 0, 0,
                                  instanceBase + static_cast<uint32_t>(i));
            ++drawn;
            i = j;
        }
    } else {
        shadowMaterial.bindMaterial(cmdBuffer);
        const Mesh* boundMesh = nullptr;
        for (const ShadowItem& item : mShadowList_) {
            if (item.mesh != boundMesh) {
                item.mesh->bindMesh(cmdBuffer);
                boundMesh = item.mesh;
            }
            const glm::mat4 mvp = lightViewProj * mWorldCache_[item.entity].world * item.element->localTransform;
            shadowMaterial.pushConstants(cmdBuffer, &mvp, sizeof(mvp), kPushStages);
            cmdBuffer.drawIndexed(item.mesh->getIndicesCount(), 1, 0, 0, 0);
            ++drawn;
        }
    }

    CLAY_PROFILE_COUNT("Draws (shadow)", drawn);
    CLAY_PROFILE_COUNT("Casters (shadow)", mShadowList_.size());
    CLAY_PROFILE_COUNT("Culled (shadow)", culled);
}

void RenderSystem::renderModel(const Model& model, vk::CommandBuffer cmdBuffer, const void* userPushData, uint32_t userPushSize) {
    // The caller's push data starts with the model matrix; each element appends its
    // own local transform to it. (Stack copy: no per-draw heap allocation.)
    std::array<uint8_t, 256> pushData{};
    assert(userPushSize >= sizeof(glm::mat4) && userPushSize <= pushData.size());

    glm::mat4 baseModel;
    std::memcpy(&baseModel, userPushData, sizeof(glm::mat4));
    std::memcpy(pushData.data(), userPushData, userPushSize);

    for (const auto& eachElement : model.getElements()) {
        Mesh& mesh = mResources_[eachElement.meshHandle];
        Material& material = mResources_[eachElement.materialHandle];

        material.bindMaterial(cmdBuffer);
        mesh.bindMesh(cmdBuffer);

        const glm::mat4 elementModel = baseModel * eachElement.localTransform;
        std::memcpy(pushData.data(), &elementModel, sizeof(glm::mat4));

        material.pushConstants(cmdBuffer, pushData.data(), userPushSize, kPushStages);
        cmdBuffer.drawIndexed(mesh.getIndicesCount(), 1, 0, 0, 0);
    }
}

} // namespace clay::ecs
