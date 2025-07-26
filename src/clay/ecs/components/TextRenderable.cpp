#include "clay/ecs/components/TextRenderable.h"

namespace clay::ecs {

TextRenderable::TextRenderable() {}

void TextRenderable::initialize(BaseGraphicsContext& gContext, const std::string& text, Font* font) {
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

    createVertexBuffer(gContext);
}

void TextRenderable::createVertexBuffer(BaseGraphicsContext& gContext) {
    vk::DeviceSize bufferSize = sizeof(mVertices_[0]) * mVertices_.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    gContext.createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data = gContext.getDevice().mapMemory(
        stagingBufferMemory, 
        0,
        bufferSize
    );
    std::memcpy(data, mVertices_.data(), static_cast<size_t>(bufferSize));
    gContext.getDevice().unmapMemory(stagingBufferMemory);

    gContext.createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer ,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mVertexBuffer_,
        mVertexBufferMemory_
    );

    gContext.copyBuffer(stagingBuffer, mVertexBuffer_, bufferSize);

    gContext.getDevice().destroyBuffer(stagingBuffer);
    gContext.getDevice().freeMemory(stagingBufferMemory);
}

void TextRenderable::finalize(BaseGraphicsContext& gContext) {
    gContext.getDevice().destroyBuffer(mVertexBuffer_);
    gContext.getDevice().freeMemory(mVertexBufferMemory_);
}

void TextRenderable::render(vk::CommandBuffer cmdBuffer, const glm::mat4& parentModelMat) {
    mpFont_->getMaterial().bindMaterial(cmdBuffer);

    struct PushConstants {
        glm::mat4 model;
        glm::vec4 color;
    } push;

    push.color = mColor_;
    push.model = parentModelMat * getModelMatrix();

    mpFont_->getMaterial().pushConstants(cmdBuffer, &push, sizeof(PushConstants), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    
    vk::Buffer vertexBuffers[] = { mVertexBuffer_ };
    vk::DeviceSize offsets[] = { 0 };

    cmdBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
    cmdBuffer.draw(static_cast<uint32_t>(mVertices_.size()), 1, 0, 0);
}

glm::mat4 TextRenderable::getModelMatrix() {
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), mPosition_);
    //rotation matrix
    glm::mat4 rotationMat = glm::mat4_cast(mOrientation_);
    // scale matrix
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), mScale_);

    return translationMat * rotationMat * scaleMat;
}


} // namespace clay::ecs