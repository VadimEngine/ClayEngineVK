#pragma once
// standard lib
#include <string>
// clay
#include "clay/graphics/common/Font.h"
#include "clay/entity/render/BaseRenderable.h"
#include "clay/graphics/common/IGraphicsContext.h"

namespace clay {

class TextRenderable : public BaseRenderable {
public:
    TextRenderable(IGraphicsContext& gContext, const std::string& text = "", Font* font = nullptr);

    ~TextRenderable();

    void initialize();

    void createVertexBuffer();

    void render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat);

    glm::mat4 getModelMatrix();

    // logic to change font and text?

    IGraphicsContext& mGraphicsContext_;

    std::string mText_;
    Font* mpFont_;

    VkBuffer mVertexBuffer_;
    VkDeviceMemory mVertexBufferMemory_;
    std::vector<Font::FontVertex> mVertices_;

    glm::vec4 mColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    // TODO use transforms from BaseRenderable instead
    glm::vec3 mPosition_ = {0.0f, 0.0f, 0.0f};
    glm::vec3 mRotation_ = { 0.0f, 0.0f, 0.0f };
    glm::vec3 mScale_ = { 1.0f, 1.0f, 1.0f };


};

} // namespace clay