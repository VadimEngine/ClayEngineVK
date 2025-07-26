#pragma once
// standard lib
#include <string>
// clay
#include "clay/graphics/common/Font.h"
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/ecs/Types.h"

namespace clay::ecs {

class TextRenderable {
public:

    static constexpr uint32_t bit = 1u << static_cast<uint32_t>(ComponentType::TEXT);

    TextRenderable();

    void initialize(BaseGraphicsContext& gContext, const std::string& text, Font* font);

    void createVertexBuffer(BaseGraphicsContext& gContext);

    void finalize(BaseGraphicsContext& gContext);

    void render(vk::CommandBuffer cmdBuffer, const glm::mat4& parentModelMat);

    glm::mat4 getModelMatrix();

    std::string mText_;
    Font* mpFont_;
    vk::Buffer mVertexBuffer_;
    vk::DeviceMemory mVertexBufferMemory_;
    std::vector<Font::FontVertex> mVertices_;
    glm::vec3 mPosition_ = {0.0f, 0.0f, 0.0f};
    glm::quat mOrientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 mScale_ { 1.0f, 1.0f, 1.0f };;
    glm::vec4 mColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace clay::ecs