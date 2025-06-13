#pragma once
// standard lib
#include <vector>
// third party
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
// clay
#include "clay/entity/render/BaseRenderable.h"

namespace clay {

class Entity {
public:

    Entity();

    ~Entity();

    virtual void render(VkCommandBuffer cmdBuffer) const;

    void addRenderable(BaseRenderable* newRenderable);

    void setEnabled(const bool isEnabled);

    bool isEnabled() const;

    /** Get this Renderable's position */
    glm::vec3 getPosition() const;

    /** Get this Renderable's orientation */
    glm::quat getOrientation() const;

    glm::quat& getOrientation();

    /** Get this Renderable's scale */
    glm::vec3 getScale() const;

    /**
     * Set this Renderable's position
     * @param newPosition New position vector
     */
    void setPosition(const glm::vec3& newPosition);

    /**
     * Set this Renderable's orientation (In degrees)
     * @param newOrientation New Orientation quaternion
     */
    void setOrientation(const glm::quat& newOrientation);

    /**
     * Set this Renderable's scale
     * @param newScale New scale vector
     */
    void setScale(const glm::vec3& newScale);

protected:
    glm::vec3 mPosition_;
    glm::quat mOrientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 mScale_;

    // TODO unique ptr
    std::vector<BaseRenderable*> mRenderables_;

    bool mEnabled_ = true;

};

} // namespace clay