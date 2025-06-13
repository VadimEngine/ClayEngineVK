#pragma once
// third party
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
// vulkan
#include <vulkan/vulkan.h>

namespace clay {

class BaseRenderable {
public:

    BaseRenderable() = default;

    virtual ~BaseRenderable() = default;

    virtual void render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat) = 0;

    void setEnabled(const bool isEnabled);

    bool isEnabled() const;

    /** Get this Renderable's position */
    glm::vec3 getPosition() const;

    /** Get this Renderable's orientation */
    glm::quat getOrientation() const;

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
    void setOrientation(const glm::vec3& newOrientation);

    /**
     * Set this Renderable's scale
     * @param newScale New scale vector
     */
    void setScale(const glm::vec3& newScale);

protected:
    glm::vec3 mPosition_ = {0.0f, 0.0f, 0.0f};;
    glm::quat mOrientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 mScale_ { 1.0f, 1.0f, 1.0f };;
    // TODO use this
    bool mEnabled_ = true;
};

} // namespace clay