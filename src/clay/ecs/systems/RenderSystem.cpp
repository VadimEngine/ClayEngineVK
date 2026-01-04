// clay
#include "clay/ecs/EntityManager.h"
#include <clay/utils/common/Logger.h>
#include "clay/graphics/common/AnimatedMesh.h"
#include "clay/graphics/common/SkeletalAnimation.h"
#include "clay/graphics/common/Model.h"
#include "clay/graphics/common/Tilemap.h"
// class
#include "clay/ecs/systems/RenderSystem.h"
// standard lib
#include <unordered_map>
#include <vector>
#include <algorithm>


namespace clay::ecs {

RenderSystem::RenderSystem(BaseGraphicsContext& gContext, Resources& resources)
    : mGContext_(gContext), mResources_(resources) {}

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

void RenderSystem::render(EntityManager& entityManager, vk::CommandBuffer cmdBuffer) {
    // Render skybox first if it exists
    if (entityManager.hasSkybox()) {
        SkyBox* skybox = entityManager.getSkybox();
        Material& material = mResources_[skybox->getMaterialHandle()];
        Mesh& mesh = mResources_[skybox->getMeshHandle()];
        
        material.bindMaterial(cmdBuffer);
        mesh.bindMesh(cmdBuffer);
        
        struct PushConstants {
            glm::mat4 model;
            glm::vec4 color;
        } push{
            .model = skybox->getModelMatrix(),
            .color = {1,1,1,1}
        };
        
        material.pushConstants(
            cmdBuffer,
            &push,
            sizeof(push),
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        );
        
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

    // Bucket entities by render layer
    std::unordered_map<int, std::vector<clay::ecs::Entity>> layerBuckets;

    for (clay::ecs::Entity e : entityManager.mCurrentEntities_) {
        if (entityManager.mSignatures[e][clay::ecs::ComponentType::METADATA] && !entityManager.mMetaData[e].enabled) {
            continue;
        }

        int layer = 0;
        
        // Models with transforms use their renderLayer
        if (entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && 
                 entityManager.mSignatures[e][clay::ecs::ComponentType::MODEL]) {
            layer = entityManager.mModelRenderable[e].renderLayer;
            layerBuckets[layer].push_back(e);
        }
        // Other renderable types default to layer 0
        else if (entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && 
                (entityManager.mSignatures[e][clay::ecs::ComponentType::TEXT] ||
                 entityManager.mSignatures[e][clay::ecs::ComponentType::SPRITE] ||
                 entityManager.mSignatures[e][clay::ecs::ComponentType::ANIMATION_2D_RENDERABLE] ||
                 entityManager.mSignatures[e][clay::ecs::ComponentType::SKELETAL_ANIMATION_RENDERABLE])) {
            layerBuckets[0].push_back(e);
        }
    }

    // Get sorted layer keys
    std::vector<int> sortedLayers;
    sortedLayers.reserve(layerBuckets.size());
    for (const auto& [layer, _] : layerBuckets) {
        sortedLayers.push_back(layer);
    }
    std::sort(sortedLayers.begin(), sortedLayers.end());

    // Render each layer in order
    for (int layer : sortedLayers) {
        for (clay::ecs::Entity e : layerBuckets[layer]) {
            // All renderables require transform
            if (!entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM]) {
                continue;
            }

            clay::ecs::Transform& transform = entityManager.mTransforms[e];

            if (entityManager.mSignatures[e][clay::ecs::ComponentType::MODEL]) {
        clay::ecs::ModelRenderable& model = entityManager.mModelRenderable[e];

        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_);
        const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);

        struct PushConstants {
            glm::mat4 model;
            glm::vec4 color; // optional depending on material
        } push{};
        push.model = translationMat * rotationMatrix * scaleMat * model.localModelMat;
        push.color = model.mColor_;

        renderModel(mResources_[model.modelHandle], cmdBuffer, &push, sizeof(push));
    } else if (entityManager.mSignatures[e][clay::ecs::ComponentType::TEXT]) {
        clay::ecs::TextRenderable& text = entityManager.mTextRenderables[e];

        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_);
        const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);

        text.mpFont_->getMaterial().bindMaterial(cmdBuffer);

        struct PushConstants {
            glm::mat4 model;
            glm::vec4 color;
        } push{
            .model = translationMat * rotationMatrix * scaleMat * glm::scale(glm::mat4(1.0f), text.mScale_),
            .color = text.mColor_
        };

        text.mpFont_->getMaterial().pushConstants(cmdBuffer, &push, sizeof(PushConstants), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

        vk::Buffer vertexBuffers[] = { text.mVertexBuffer_ };
        vk::DeviceSize offsets[] = { 0 };

        cmdBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
        cmdBuffer.draw(static_cast<uint32_t>(text.mVertices_.size()), 1, 0, 0);

    } else if (entityManager.mSignatures[e][clay::ecs::ComponentType::SPRITE]) {
        clay::ecs::SpriteRenderable& sprite = entityManager.mSpriteRenderables[e];

        Material& material = mResources_[sprite.materialHandle];
        Mesh& mesh = mResources_[sprite.meshHandle];

        material.bindMaterial(cmdBuffer);

        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_);
        const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);

        struct PushConstants {
            glm::mat4 model;
            glm::vec4 color;
            glm::vec4 offsets;
        } push{};

        push.model = translationMat * rotationMatrix * scaleMat;
        push.color = sprite.mColor_;
        push.offsets = sprite.mSpriteOffset_;

        material.pushConstants(
            cmdBuffer,
            &push,
            sizeof(push),
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        );

        mesh.bindMesh(cmdBuffer);
        cmdBuffer.drawIndexed(mesh.getIndicesCount(), 1, 0, 0, 0);
    } else if (entityManager.mSignatures[e][clay::ecs::ComponentType::ANIMATION_2D_RENDERABLE]) {
        // Render Animation2DRenderable
        clay::ecs::Animation2DRenderable& animRenderable = entityManager.mAnimation2DRenderables[e];
        const Animation2D& animation = mResources_[animRenderable.animationHandle];

        // Bind the animation's material (spritesheet)
        Material& material = mResources_[animation.materialHandle];
        material.bindMaterial(cmdBuffer);

        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_ + animRenderable.mOffset_);
        const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);

        struct PushConstants {
            glm::mat4 model;
            glm::vec4 color;
            glm::vec4 offsets;
        } push{
            .model = translationMat * rotationMatrix * scaleMat,
            .color = animRenderable.mColor_,
            // Use current frame from animation
            .offsets = !animation.frames.empty() ? animation.frames[animRenderable.currentFrame] : glm::vec4(0)
        };

        material.pushConstants(
            cmdBuffer,
            &push,
            sizeof(push),
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        );

        Mesh& mesh = mResources_[animRenderable.meshHandle];
        mesh.bindMesh(cmdBuffer);
        cmdBuffer.drawIndexed(mesh.getIndicesCount(), 1, 0, 0, 0);
    } else if (entityManager.mSignatures[e][clay::ecs::ComponentType::SKELETAL_ANIMATION_RENDERABLE]) {
        // Render Animation3DRenderable
        clay::ecs::Animation3DRenderable& animRenderable = entityManager.mAnimation3DRenderables[e];
        
        AnimatedMesh& mesh = mResources_[animRenderable.meshHandle];
        Material& material = mResources_[animRenderable.materialHandle];
        
        // Bind material
        material.bindMaterial(cmdBuffer);
        
        // Calculate model matrix
        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_);
        const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);
        
        struct PushConstants {
            glm::mat4 model;
            glm::vec4 color;
        } push{};
        
        push.model = translationMat * rotationMatrix * scaleMat;
        push.color = animRenderable.mColor_;
        
        material.pushConstants(
            cmdBuffer,
            &push,
            sizeof(push),
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        );
        
        // Bind mesh and draw
        mesh.bindMesh(cmdBuffer);
        cmdBuffer.drawIndexed(mesh.getIndicesCount(), 1, 0, 0, 0);
            }
        }
    }

    // Debug draw colliders
    if (mDebugDrawColliders_) {
        Material& debugMaterial = mResources_[mDebugMaterialHandle_];
        Mesh& planeMesh = mResources_[mDebugMeshHandle_];
        
        for (clay::ecs::Entity e : entityManager.mCurrentEntities_) {
            if (!entityManager.mSignatures[e][clay::ecs::ComponentType::COLLIDER] ||
                !entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM]) {
                continue;
            }
            
            const auto& transform = entityManager.mTransforms[e];
            const auto& collider = entityManager.mColliders[e];
            
            debugMaterial.bindMaterial(cmdBuffer);
            
            glm::vec3 colliderPos = transform.mPosition_ + collider.offset;
            
            if (collider.mType_ == Collider::Type::AABB) {
                // Draw 4 edges of AABB as thin rectangles
                // Apply scale to match collision detection world space
                glm::vec3 halfExtents = collider.aabb.halfExtents * transform.mScale_;
                float lineThickness = 0.02f;
                
                // Top edge
                {
                    glm::vec3 pos = colliderPos + glm::vec3(0, halfExtents.y, 0);
                    glm::vec3 scale = glm::vec3(halfExtents.x * 2.0f, lineThickness, 1.0f);
                    
                    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
                    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                    
                    struct PushConstants {
                        glm::mat4 model;
                        glm::vec4 color;
                    } push{
                        .model = translationMat * scaleMat,
                        .color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) // Green
                    };
                    
                    debugMaterial.pushConstants(
                        cmdBuffer, 
                        &push, 
                        sizeof(push),
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                    );
                    planeMesh.bindMesh(cmdBuffer);
                    cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                }
                
                // Bottom edge
                {
                    glm::vec3 pos = colliderPos + glm::vec3(0, -halfExtents.y, 0);
                    glm::vec3 scale = glm::vec3(halfExtents.x * 2.0f, lineThickness, 1.0f);
                    
                    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
                    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                    
                    struct PushConstants {
                        glm::mat4 model;
                        glm::vec4 color;
                    } push{
                        .model = translationMat * scaleMat,
                        .color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) // Green
                    };
                    
                    debugMaterial.pushConstants(
                        cmdBuffer, 
                        &push, 
                        sizeof(push),
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                    );
                    planeMesh.bindMesh(cmdBuffer);
                    cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                }
                
                // Left edge
                {
                    glm::vec3 pos = colliderPos + glm::vec3(-halfExtents.x, 0, 0);
                    glm::vec3 scale = glm::vec3(lineThickness, halfExtents.y * 2.0f, 1.0f);
                    
                    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
                    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                    
                    struct PushConstants {
                        glm::mat4 model;
                        glm::vec4 color;
                    } push{
                        .model = translationMat * scaleMat,
                        .color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) // Green
                    };
                    
                    debugMaterial.pushConstants(
                        cmdBuffer, 
                        &push, 
                        sizeof(push),
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                    );
                    planeMesh.bindMesh(cmdBuffer);
                    cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                }
                
                // Right edge
                {
                    glm::vec3 pos = colliderPos + glm::vec3(halfExtents.x, 0, 0);
                    glm::vec3 scale = glm::vec3(lineThickness, halfExtents.y * 2.0f, 1.0f);
                    
                    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
                    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                    
                    struct PushConstants {
                        glm::mat4 model;
                        glm::vec4 color;
                    } push{
                        .model = translationMat * scaleMat,
                        .color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) // Green
                    };
                    
                    debugMaterial.pushConstants(
                        cmdBuffer,
                        &push, 
                        sizeof(push),
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                    );
                    planeMesh.bindMesh(cmdBuffer);
                    cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                }
            } else if (collider.mType_ == Collider::Type::CIRCLE) {
                // Draw circle as a scaled quad
                glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), colliderPos);
                glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(collider.circle.radius * 2.0f, collider.circle.radius * 2.0f, 1.0f));
                
                struct PushConstants {
                    glm::mat4 model;
                    glm::vec4 color;
                } push{
                    .model = translationMat * scaleMat,
                    .color = glm::vec4(0.0f, 1.0f, 0.0f, 0.3f) // Green, semi-transparent
                };
                
                debugMaterial.pushConstants(
                    cmdBuffer, 
                    &push, 
                    sizeof(push),
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                );
                planeMesh.bindMesh(cmdBuffer);
                cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
            }
        }
        
        // Debug draw PhysX rigid body colliders
        for (clay::ecs::Entity e : entityManager.mCurrentEntities_) {
            if (!entityManager.mSignatures[e][clay::ecs::ComponentType::PHYSX_RIGID_BODY] ||
                !entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM]) {
                continue;
            }
            
            const auto& rigidBody = entityManager.mPhysXRigidBodies[e];
            const auto& transform = entityManager.mTransforms[e];
            
            if (!rigidBody.actor) continue;
            
            // Get all shapes attached to this actor
            physx::PxU32 numShapes = rigidBody.actor->getNbShapes();
            if (numShapes == 0) continue;
            
            std::vector<physx::PxShape*> shapes(numShapes);
            rigidBody.actor->getShapes(shapes.data(), numShapes);
            
            debugMaterial.bindMaterial(cmdBuffer);
            
            for (physx::PxShape* shape : shapes) {
                physx::PxGeometryHolder geomHolder = shape->getGeometry();
                physx::PxTransform localPose = shape->getLocalPose();
                
                // Convert PhysX local pose to glm offset
                glm::vec3 shapeOffset(localPose.p.x, localPose.p.y, localPose.p.z);
                glm::vec3 colliderPos = transform.mPosition_ + shapeOffset;
                
                if (geomHolder.getType() == physx::PxGeometryType::eBOX) {
                    physx::PxBoxGeometry& box = geomHolder.box();
                    glm::vec3 halfExtents(box.halfExtents.x, box.halfExtents.y, box.halfExtents.z);
                    float lineThickness = 0.02f;
                    
                    // Draw 4 edges of box as thin rectangles
                    // Top edge
                    {
                        glm::vec3 pos = colliderPos + glm::vec3(0, halfExtents.y, 0);
                        glm::vec3 scale = glm::vec3(halfExtents.x * 2.0f, lineThickness, 1.0f);
                        
                        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
                        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                        
                        struct PushConstants {
                            glm::mat4 model;
                            glm::vec4 color;
                        } push{
                            .model = translationMat * scaleMat,
                            .color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) // Magenta for PhysX
                        };
                        
                        debugMaterial.pushConstants(
                            cmdBuffer, 
                            &push, 
                            sizeof(push),
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                        );
                        planeMesh.bindMesh(cmdBuffer);
                        cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                    }
                    
                    // Bottom edge
                    {
                        glm::vec3 pos = colliderPos + glm::vec3(0, -halfExtents.y, 0);
                        glm::vec3 scale = glm::vec3(halfExtents.x * 2.0f, lineThickness, 1.0f);
                        
                        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
                        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                        
                        struct PushConstants {
                            glm::mat4 model;
                            glm::vec4 color;
                        } push{
                            .model = translationMat * scaleMat,
                            .color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) // Magenta for PhysX
                        };
                        
                        debugMaterial.pushConstants(
                            cmdBuffer,
                            &push,
                            sizeof(push),
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                        );
                        planeMesh.bindMesh(cmdBuffer);
                        cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                    }
                    
                    // Left edge
                    {
                        glm::vec3 pos = colliderPos + glm::vec3(-halfExtents.x, 0, 0);
                        glm::vec3 scale = glm::vec3(lineThickness, halfExtents.y * 2.0f, 1.0f);
                        
                        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
                        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                        
                        struct PushConstants {
                            glm::mat4 model;
                            glm::vec4 color;
                        } push{
                            .model = translationMat * scaleMat,
                            .color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) // Magenta for PhysX
                        };
                        
                        debugMaterial.pushConstants(cmdBuffer, &push, sizeof(push),
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
                        planeMesh.bindMesh(cmdBuffer);
                        cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                    }
                    
                    // Right edge
                    {
                        glm::vec3 pos = colliderPos + glm::vec3(halfExtents.x, 0, 0);
                        glm::vec3 scale = glm::vec3(lineThickness, halfExtents.y * 2.0f, 1.0f);
                        
                        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), pos);
                        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                        
                        struct PushConstants {
                            glm::mat4 model;
                            glm::vec4 color;
                        } push{
                            .model = translationMat * scaleMat,
                            .color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) // Magenta for PhysX
                        };

                        debugMaterial.pushConstants(cmdBuffer, &push, sizeof(push),
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
                        planeMesh.bindMesh(cmdBuffer);
                        cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                    }
                } else if (geomHolder.getType() == physx::PxGeometryType::eSPHERE) {
                    physx::PxSphereGeometry& sphere = geomHolder.sphere();
                    
                    // Draw sphere as a scaled quad
                    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), colliderPos);
                    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(sphere.radius * 2.0f, sphere.radius * 2.0f, 1.0f));
                    
                    struct PushConstants {
                        glm::mat4 model;
                        glm::vec4 color;
                    } push{
                        .model = translationMat * scaleMat,
                        .color = glm::vec4(1.0f, 0.0f, 1.0f, 0.3f) // Magenta, semi-transparent
                    };

                    debugMaterial.pushConstants(
                        cmdBuffer,
                        &push,
                        sizeof(push),
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                    );
                    planeMesh.bindMesh(cmdBuffer);
                    cmdBuffer.drawIndexed(planeMesh.getIndicesCount(), 1, 0, 0, 0);
                }
            }
        }
    }
}

void RenderSystem::renderModel(const Model& model, vk::CommandBuffer cmdBuffer, const void* userPushData, uint32_t userPushSize) {
    for (const auto& eachElement : model.getElements()) {
        Mesh& mesh = mResources_[eachElement.meshHandle];
        Material& material = mResources_[eachElement.materialHandle];

        material.bindMaterial(cmdBuffer);
        mesh.bindMesh(cmdBuffer);

        // make a copy of instance data
        std::vector<uint8_t> pushDataCopy(userPushSize);
        std::memcpy(pushDataCopy.data(), userPushData, userPushSize);
        // Assume model is in the beginning
        glm::mat4* modelMat = reinterpret_cast<glm::mat4*>(pushDataCopy.data());
        *modelMat = (*modelMat) * eachElement.localTransform;

        material.pushConstants(
            cmdBuffer,
            pushDataCopy.data(),
            userPushSize,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        );

        cmdBuffer.drawIndexed(mesh.getIndicesCount(), 1, 0, 0, 0);
    }
}

} // namespace clay::ecs