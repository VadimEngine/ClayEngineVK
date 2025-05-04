// standard lib
#include <stdexcept>
// third party
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/gtc/type_ptr.hpp>
// clay
#include "clay/utils/common/Logger.h"
// class
#include "clay/graphics/common/Font.h"

namespace clay {

VkVertexInputBindingDescription Font::FontVertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(FontVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> Font::FontVertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    // vertex (vec4 -> 4 floats)
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(FontVertex, vertex);

    // glyphIndex (int)
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32_SINT;
    attributeDescriptions[1].offset = offsetof(FontVertex, glyphIndex);

    return attributeDescriptions;
}

// temp populate method
void populateImage(VkImage image, const FT_Bitmap& bitmap, IGraphicsContext& mGContext_) {
    // Assuming bitmap.pixel_mode == FT_PIXEL_MODE_GRAY and bitmap.num_grays == 256

    VkDeviceSize imageSize = bitmap.width * bitmap.rows; // 1 byte per pixel, single channel

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    mGContext_.createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(mGContext_.mDevice_, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, bitmap.buffer, static_cast<size_t>(imageSize));
    vkUnmapMemory(mGContext_.mDevice_, stagingBufferMemory);

    mGContext_.transitionImageLayout(
        image,
        VK_FORMAT_R8_UNORM,              // single channel 8-bit unsigned normalized
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1
    );

    mGContext_.copyBufferToImage(
        stagingBuffer,
        image,
        static_cast<uint32_t>(bitmap.width),
        static_cast<uint32_t>(bitmap.rows)
    );

    mGContext_.transitionImageLayout(
        image,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1
    );

    vkDestroyBuffer(mGContext_.mDevice_, stagingBuffer, nullptr);
    vkFreeMemory(mGContext_.mDevice_, stagingBufferMemory, nullptr);
}

Font::Font(IGraphicsContext& gContext, utils::FileData& fileData, utils::FileData& vertexFileData, utils::FileData& fragmentFileData)
    : mGContext_(gContext){

    mCharacterImage_.fill(VK_NULL_HANDLE);
    mCharacterMemory_.fill(VK_NULL_HANDLE);
    mCharacterImageView_.fill(VK_NULL_HANDLE);

    // Initialize the FreeType library
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        LOG_E("ERROR::FREETYPE::Could not init FreeType Library");
        return;
    }

    FT_Face face;
    FT_Error error = FT_New_Memory_Face(
        ft,
        reinterpret_cast<const FT_Byte*>(fileData.data.get()),
        static_cast<FT_Long>(fileData.size),
        0,
        &face
    );

    if (error) {
        LOG_E("ERROR::FREETYPE::Failed to load font from memory. Error code: %d", error);
        FT_Done_FreeType(ft);
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);

    // TODO create pipeline and descriptor set

    for (unsigned char c = 0; c < 128; ++c) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            LOG_E("ERROR::FREETYTPE: Failed to load Glyph");
            continue;
        }

        const FT_Bitmap& bitmap = face->glyph->bitmap;

        mCharacterFrontInfo_[c] = {
            bitmap.width,
            bitmap.rows,
            face->glyph->bitmap_left,
            face->glyph->bitmap_top,
            static_cast<uint32_t>(face->glyph->advance.x)
        };

        if (bitmap.width == 0 || bitmap.rows == 0) {
            continue; // Skip glyphs with no visible bitmap
            // TODO still update mCharacterFrontInfo_
        }

        // 1. Create VkImage for this glyph
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8_UNORM;  // 1 channel 8-bit grayscale
        imageInfo.extent.width = bitmap.width;
        imageInfo.extent.height = bitmap.rows;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage glyphImage;
        if (vkCreateImage(mGContext_.mDevice_, &imageInfo, nullptr, &glyphImage) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkDeviceMemory imageMemory;

        // 2. Allocate memory for the image, bind memory (you need to find memory type and allocate)
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(mGContext_.mDevice_, glyphImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = mGContext_.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(mGContext_.mDevice_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(mGContext_.mDevice_, glyphImage, imageMemory, 0);

        // 3. Create VkImageView for the image
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = glyphImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView glyphImageView;
        vkCreateImageView(mGContext_.mDevice_, &viewInfo, nullptr, &glyphImageView);

        populateImage(glyphImage, bitmap, mGContext_);

        mCharacterImage_[c] = glyphImage;
        mCharacterMemory_[c] = imageMemory;
        mCharacterImageView_[c] = glyphImageView;
    }

    createPipeline(vertexFileData, fragmentFileData);
}

Font::~Font() {}

void Font::createPipeline(utils::FileData& vertexFileData, utils::FileData& fragmentFileData) {
    VkShaderModule vertexShader = mGContext_.createShader(
        {VK_SHADER_STAGE_VERTEX_BIT, vertexFileData.data.get(), vertexFileData.size}
    );
    VkShaderModule fragmentShader = mGContext_.createShader(
        {VK_SHADER_STAGE_FRAGMENT_BIT, fragmentFileData.data.get(), fragmentFileData.size}
    );

    clay::PipelineResource::PipelineConfig pipelineConfig{
        .graphicsContext = mGContext_
    };

    pipelineConfig.shaderStages = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShader,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShader,
            .pName = "main"
        }
    };
    auto vertexAttrib = Font::FontVertex::getAttributeDescriptions();
    pipelineConfig.attributeDescriptions = {vertexAttrib.begin(), vertexAttrib.end()};
    pipelineConfig.vertexInputBindingDescription = Font::FontVertex::getBindingDescription();

    pipelineConfig.pushConstants = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(glm::mat4)
        },
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 64,
            .size = sizeof(glm::vec4)
        }
    };

    pipelineConfig.bufferCreateInfos = {
        {
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .stride = 0,
            .size = sizeof(Font::CameraConstant),
            .data = nullptr,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .binding = 0
        },
    };

    pipelineConfig.imageCreateInfos = {
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 128,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        }
    };

    pipelineConfig.depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    pipelineConfig.rasterizerState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    mPipeline_ = new PipelineResource(pipelineConfig);

    clay::Material::MaterialConfig matConfig {
        .graphicsContext = mGContext_,
        .pipelineResource = *mPipeline_
    };

    matConfig.bufferCreateInfos = {
        {
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .stride = 0,
            .size = sizeof(Font::CameraConstant),
            .data = nullptr,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .binding = 0
        },
    };

    VkSampler sampler = mGContext_.createSampler();

    for (int i = 0; i < 128; ++i) {
        matConfig.imageCreateInfosArray.push_back(
            {
                .data = nullptr,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .imageView = mCharacterImageView_[i] != VK_NULL_HANDLE ? mCharacterImageView_[i] : mCharacterImageView_['a'], // TODO fix this
                .sampler = sampler,
                .binding = 1
            }
        );
    }

    mMaterial_ = new Material(matConfig);
}

void Font::createVertexBuffer() {
//    VkDeviceSize bufferSize = sizeof(mVertices_[0]) * mVertices_.size();
//
//    VkBuffer stagingBuffer;
//    VkDeviceMemory stagingBufferMemory;
//    mGContext_.createBuffer(
//        bufferSize,
//        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//        stagingBuffer,
//        stagingBufferMemory
//    );
//
//    void* data;
//    vkMapMemory(mGContext_.mDevice_, stagingBufferMemory, 0, bufferSize, 0, &data);
//    memcpy(data, mVertices_.data(), (size_t) bufferSize);
//    vkUnmapMemory(mGContext_.mDevice_, stagingBufferMemory);
//
//    mGContext_.createBuffer(
//        bufferSize,
//        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
//        mVertexBuffer_,
//        mVertexBufferMemory_
//    );
//
//    mGContext_.copyBuffer(stagingBuffer, mVertexBuffer_, bufferSize);
//
//    vkDestroyBuffer(mGContext_.mDevice_, stagingBuffer, nullptr);
//    vkFreeMemory(mGContext_.mDevice_, stagingBufferMemory, nullptr);
}

void Font::initTemp(const std::string& str) {
//    mString_ = str;
//
//    float x = 0.0f; // starting x position
//    float y = 0.0f; // baseline y position
//
//    for (char c : mString_) {
//        const CharacterInfo& glyph = mCharacterFrontInfo_[static_cast<uint8_t>(c)];
//        if (glyph.width == 0 || glyph.height == 0) {
//            x += glyph.advance / 64.0f; // Skip empty glyphs
//            continue;
//        }
//
//        float xpos = x + static_cast<float>(glyph.bitmapLeft);
//        float ypos = y + static_cast<float>(glyph.bitmapTop) - static_cast<float>(glyph.height);
//
//        float w = static_cast<float>(glyph.width);
//        float h = static_cast<float>(glyph.height);
//
//        // Normalized tex coords for full glyph image
//        float u0 = 0.0f, v0 = 1.0f;
//        float u1 = 1.0f, v1 = 0.0f;
//
//        int glyphIndex = static_cast<int>(c);
//
//        // Triangle 1
//        mVertices_.push_back({ glm::vec4(xpos,     ypos + h, u0, v1), glyphIndex });
//        mVertices_.push_back({ glm::vec4(xpos,     ypos,     u0, v0), glyphIndex });
//        mVertices_.push_back({ glm::vec4(xpos + w, ypos,     u1, v0), glyphIndex });
//
//        // Triangle 2
//        mVertices_.push_back({ glm::vec4(xpos,     ypos + h, u0, v1), glyphIndex });
//        mVertices_.push_back({ glm::vec4(xpos + w, ypos,     u1, v0), glyphIndex });
//        mVertices_.push_back({ glm::vec4(xpos + w, ypos + h, u1, v1), glyphIndex });
//
//        x += glyph.advance / 64.0f; // advance in pixels
//    }
//
//    // Upload to a vertex buffer (you may cache this)
//    createVertexBuffer(); // your helper
}

void Font::render(VkCommandBuffer cmdBuffer) {
//    // Bind pipeline and resources
//    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline_->mPipeline_);
//
//    VkBuffer vertexBuffers[] = { mVertexBuffer_ };
//    VkDeviceSize offsets[] = { 0 };
//    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
//
//    vkCmdBindDescriptorSets(
//        cmdBuffer,
//        VK_PIPELINE_BIND_POINT_GRAPHICS,
//        mPipeline_->mPipelineLayout_,
//        0, // descriptor set 1 (glyph texture array)
//        1,
//        &mMaterial_->mDescriptorSet_,
//        0,
//        nullptr
//    );
//
//    // Push constants (model and color)
//    glm::mat4 model = getModelMatrix();
//    glm::vec4 color = glm::vec4(1,1,1,1);
//
//    vkCmdPushConstants(cmdBuffer, mPipeline_->mPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
//    vkCmdPushConstants(cmdBuffer, mPipeline_->mPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec4),&color);
//
//    // Issue draw call
//    vkCmdDraw(cmdBuffer, static_cast<uint32_t>(mVertices_.size()), 1, 0, 0);
}

glm::mat4 Font::getModelMatrix() {
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), mPosition_);
    //rotation matrix
    glm::mat4 rotationMat = glm::rotate(glm::mat4(1), glm::radians(mRotation_.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotationMat = glm::rotate(rotationMat, glm::radians(mRotation_.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotationMat = glm::rotate(rotationMat, glm::radians(mRotation_.z), glm::vec3(0.0f, 0.0f, 1.0f));
    // scale matrix
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), mScale_);

    return translationMat * rotationMat * scaleMat;
}

} // namespace clay