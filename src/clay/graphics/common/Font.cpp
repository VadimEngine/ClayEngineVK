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

// temp populate method TODO merge this into GraphicsContext
void populateImage(VkImage image, const FT_Bitmap& bitmap, BaseGraphicsContext& mGContext_) {
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
    vkMapMemory(mGContext_.getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, bitmap.buffer, static_cast<size_t>(imageSize));
    vkUnmapMemory(mGContext_.getDevice(), stagingBufferMemory);

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

    vkDestroyBuffer(mGContext_.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(mGContext_.getDevice(), stagingBufferMemory, nullptr);
}

Font::Font(BaseGraphicsContext& gContext, utils::FileData& fontFileData, utils::FileData& vertexFileData, utils::FileData& fragmentFileData, UniformBuffer& uniformBuffer)
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
        reinterpret_cast<const FT_Byte*>(fontFileData.data.get()),
        static_cast<FT_Long>(fontFileData.size),
        0,
        &face
    );

    if (error) {
        LOG_E("ERROR::FREETYPE::Failed to load font from memory. Error code: %d", error);
        FT_Done_FreeType(ft);
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);


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
        if (vkCreateImage(mGContext_.getDevice(), &imageInfo, nullptr, &glyphImage) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkDeviceMemory imageMemory;

        // 2. Allocate memory for the image, bind memory (you need to find memory type and allocate)
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(mGContext_.getDevice(), glyphImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = mGContext_.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(mGContext_.getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(mGContext_.getDevice(), glyphImage, imageMemory, 0);

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
        vkCreateImageView(mGContext_.getDevice(), &viewInfo, nullptr, &glyphImageView);

        populateImage(glyphImage, bitmap, mGContext_);

        mCharacterImage_[c] = glyphImage;
        mCharacterMemory_[c] = imageMemory;
        mCharacterImageView_[c] = glyphImageView;
    }

    createPipeline(vertexFileData, fragmentFileData, uniformBuffer);
}

Font::~Font() {
    for (size_t i = 0; i < 128; ++i) {
        if (mCharacterImageView_[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(mGContext_.getDevice(), mCharacterImageView_[i], nullptr);
            mCharacterImageView_[i] = VK_NULL_HANDLE;
        }

        if (mCharacterImage_[i] != VK_NULL_HANDLE) {
            vkDestroyImage(mGContext_.getDevice(), mCharacterImage_[i], nullptr);
            mCharacterImage_[i] = VK_NULL_HANDLE;
        }

        if (mCharacterMemory_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(mGContext_.getDevice(), mCharacterMemory_[i], nullptr);
            mCharacterMemory_[i] = VK_NULL_HANDLE;
        }
    }
    vkDestroySampler(mGContext_.getDevice(), mSampler_, nullptr);
}

const PipelineResource& Font::getPipeline() const {
    return *mPipeline_;
}

const Material& Font::getMaterial() const {
    return *mMaterial_;
}

const Font::CharacterInfo& Font::getCharacterInfo(char c) const {
    return mCharacterFrontInfo_[static_cast<int>(c)];
}

void Font::createPipeline(utils::FileData& vertexFileData, utils::FileData& fragmentFileData, UniformBuffer& uniformBuffer) {
    VkShaderModule vertexShader = mGContext_.createShader(
        {VK_SHADER_STAGE_VERTEX_BIT, vertexFileData.data.get(), vertexFileData.size}
    );
    VkShaderModule fragmentShader = mGContext_.createShader(
        {VK_SHADER_STAGE_FRAGMENT_BIT, fragmentFileData.data.get(), fragmentFileData.size}
    );

    clay::PipelineResource::PipelineConfig pipelineConfig{
        .graphicsContext = mGContext_
    };

    pipelineConfig.pipelineLayoutInfo.shaderStages = {
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
    pipelineConfig.pipelineLayoutInfo.attributeDescriptions = {vertexAttrib.begin(), vertexAttrib.end()};
    pipelineConfig.pipelineLayoutInfo.vertexInputBindingDescription = Font::FontVertex::getBindingDescription();

    pipelineConfig.pipelineLayoutInfo.depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    pipelineConfig.pipelineLayoutInfo.rasterizerState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    pipelineConfig.pipelineLayoutInfo.pushConstants = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(glm::mat4) + sizeof(glm::vec4)
        }
    };

    pipelineConfig.bindingLayoutInfo.bindings = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 128,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        }
    };


    mPipeline_ = std::make_unique<PipelineResource>(pipelineConfig);

    Material::MaterialConfig matConfig {
        .graphicsContext = mGContext_,
        .pipelineResource = *mPipeline_
    };

    matConfig.bufferBindings = {
        {
            .buffer = uniformBuffer.mBuffer_,
            .size = uniformBuffer.getSize_(),
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        }
    };


    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.mipLodBias = 0.0f;

    if (vkCreateSampler(mGContext_.getDevice(), &samplerInfo, nullptr, &mSampler_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    for (int i = 0; i < 128; ++i) {
        matConfig.imageArrayBindings.push_back(
            {
                .sampler = mSampler_,
                .imageView = mCharacterImageView_[i] != VK_NULL_HANDLE ? mCharacterImageView_[i] : mCharacterImageView_['a'], // TODO fix this
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            }
        );
    }

    mMaterial_ = std::make_unique<Material>(matConfig);

    vkDestroyShaderModule(mGContext_.getDevice(), vertexShader, nullptr);
    vkDestroyShaderModule(mGContext_.getDevice(), fragmentShader, nullptr);
}

} // namespace clay