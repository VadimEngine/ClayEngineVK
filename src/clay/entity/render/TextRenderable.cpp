#include <cstring> // memcpy
// class
#include "clay/entity/render/TextRenderable.h"

namespace clay {

TextRenderable::TextRenderable(BaseGraphicsContext& gContext, const std::string& text, Font* font)
    : BaseRenderable(), mGraphicsContext_(gContext) {
    mText_ = text;
    mpFont_ = font;
    float totalWidth = 0.0f;
    for (const char& c : mText_) {
        const Font::CharacterInfo& glyph = mpFont_->getCharacterInfo(c);
        totalWidth += (glyph.advance >> 6);
    }

    float x =  -totalWidth / 2.0f; // starting x position
    float y = 0.0f; // baseline y position

    for (char c : mText_) {
        const Font::CharacterInfo& glyph = mpFont_->getCharacterInfo(c);
        if (glyph.width == 0 || glyph.height == 0) {
            x += glyph.advance / 64.0f; // Skip empty glyphs
            continue;
        }

        float xpos = x + static_cast<float>(glyph.bitmapLeft);
        float ypos = y + static_cast<float>(glyph.bitmapTop) - static_cast<float>(glyph.height);

        float w = static_cast<float>(glyph.width);
        float h = static_cast<float>(glyph.height);

        // Normalized tex coords for full glyph image
        float u0 = 0.0f, v0 = 1.0f;
        float u1 = 1.0f, v1 = 0.0f;

        int glyphIndex = static_cast<int>(c);

        // Triangle 1
        mVertices_.push_back({ glm::vec4(xpos,     ypos + h, u0, v1), glyphIndex });
        mVertices_.push_back({ glm::vec4(xpos,     ypos,     u0, v0), glyphIndex });
        mVertices_.push_back({ glm::vec4(xpos + w, ypos,     u1, v0), glyphIndex });

        // Triangle 2
        mVertices_.push_back({ glm::vec4(xpos,     ypos + h, u0, v1), glyphIndex });
        mVertices_.push_back({ glm::vec4(xpos + w, ypos,     u1, v0), glyphIndex });
        mVertices_.push_back({ glm::vec4(xpos + w, ypos + h, u1, v1), glyphIndex });

        x += glyph.advance / 64.0f; // advance in pixels
    }

    createVertexBuffer();
}

TextRenderable::~TextRenderable() {
    vkDestroyBuffer(mGraphicsContext_.getDevice(), mVertexBuffer_, nullptr);
    vkFreeMemory(mGraphicsContext_.getDevice(), mVertexBufferMemory_, nullptr);
}

void TextRenderable::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(mVertices_[0]) * mVertices_.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    mGraphicsContext_.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(mGraphicsContext_.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, mVertices_.data(), (size_t)bufferSize);
    vkUnmapMemory(mGraphicsContext_.getDevice(), stagingBufferMemory);

    mGraphicsContext_.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mVertexBuffer_,
        mVertexBufferMemory_
    );

    mGraphicsContext_.copyBuffer(stagingBuffer, mVertexBuffer_, bufferSize);

    vkDestroyBuffer(mGraphicsContext_.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(mGraphicsContext_.getDevice(), stagingBufferMemory, nullptr);
}

void TextRenderable::render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat) {
    mpFont_->getMaterial().bindMaterial(cmdBuffer);

    struct PushConstants {
        glm::mat4 model;
        glm::vec4 color;
    } push;

    push.color = mColor_;
    push.model = parentModelMat * getModelMatrix();

    mpFont_->getMaterial().pushConstants(cmdBuffer, &push, sizeof(PushConstants),  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkBuffer vertexBuffers[] = { mVertexBuffer_ };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdDraw(cmdBuffer, static_cast<uint32_t>(mVertices_.size()), 1, 0, 0);
}

glm::mat4 TextRenderable::getModelMatrix() {
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), mPosition_);
    //rotation matrix
    glm::mat4 rotationMat = glm::mat4_cast(mOrientation_);
    // scale matrix
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), mScale_);

    return translationMat * rotationMat * scaleMat;
}

void TextRenderable::setColor(const glm::vec4 newColor) {
    mColor_ = newColor;
}

} // namespace clay