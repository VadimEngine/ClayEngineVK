#pragma once
// standard lib
#include <string>
// clay
#include "clay/graphics/common/Font.h"
#include "clay/entity/render/BaseRenderable.h"
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

class TextRenderable : public BaseRenderable {
public:
    TextRenderable(BaseGraphicsContext& gContext, const std::string& text = "", Font* font = nullptr);

    ~TextRenderable();

    void createVertexBuffer();

    void render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat);

    glm::mat4 getModelMatrix();

    void setColor(const glm::vec4 newColor);

    // logic to change font and text?

private:
    BaseGraphicsContext& mGraphicsContext_;

    std::string mText_;
    Font* mpFont_;

    VkBuffer mVertexBuffer_;
    VkDeviceMemory mVertexBufferMemory_;
    std::vector<Font::FontVertex> mVertices_;

    glm::vec4 mColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace clay