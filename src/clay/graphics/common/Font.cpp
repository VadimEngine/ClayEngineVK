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

vk::VertexInputBindingDescription Font::FontVertex::getBindingDescription() {
    return {
        .binding = 0,
        .stride = sizeof(FontVertex),
        .inputRate = vk::VertexInputRate::eVertex,
    };
}

std::array<vk::VertexInputAttributeDescription, 2> Font::FontVertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};
    // vertex (vec4)
    attributeDescriptions[0] = {
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = offsetof(FontVertex, vertex)
    };

    // glyphIndex (int)
    attributeDescriptions[1] = {
       .location = 1,
       .binding = 0,
       .format = vk::Format::eR32Sint,
       .offset = offsetof(FontVertex, glyphIndex)
    };

    return attributeDescriptions;
}

// temp populate method TODO merge this into GraphicsContext
void populateImage(vk::Image image, const FT_Bitmap& bitmap, BaseGraphicsContext& mGContext_) {
    // Assuming bitmap.pixel_mode == FT_PIXEL_MODE_GRAY and bitmap.num_grays == 256
    vk::DeviceSize imageSize = bitmap.width * bitmap.rows; // 1 byte per pixel, single channel

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    mGContext_.createBuffer(
        imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data = mGContext_.getDevice().mapMemory(stagingBufferMemory, 0, imageSize);
    memcpy(data, bitmap.buffer, static_cast<size_t>(imageSize));
    mGContext_.getDevice().unmapMemory(stagingBufferMemory);
    mGContext_.transitionImageLayout(
        image,
        vk::Format::eR8Unorm,              // single channel 8-bit unsigned normalized
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
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
        vk::Format::eR8Unorm,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        1
    );

    mGContext_.getDevice().destroyBuffer(stagingBuffer);
    mGContext_.getDevice().freeMemory(stagingBufferMemory);
}

Font::Font(BaseGraphicsContext& gContext, utils::FileData& fontFileData, ShaderModule& vertShader, ShaderModule& fragShader, UniformBuffer& uniformBuffer)
    : mGContext_(gContext) {

    mCharacterImage_.fill(nullptr);
    mCharacterMemory_.fill(nullptr);
    mCharacterImageView_.fill(nullptr);

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
            .width = bitmap.width,
            .height = bitmap.rows,
            .bitmapLeft = face->glyph->bitmap_left,
            .bitmapTop = face->glyph->bitmap_top,
            .advance = static_cast<uint32_t>(face->glyph->advance.x)
        };

        if (bitmap.width == 0 || bitmap.rows == 0) {
            continue; // Skip glyphs with no visible bitmap
        }

        // 1. Create vk::Image for this glyph
        vk::ImageCreateInfo imageInfo{
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eR8Unorm,  // 1 channel 8-bit grayscale
            .extent = vk::Extent3D{ static_cast<uint32_t>(bitmap.width), static_cast<uint32_t>(bitmap.rows), 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        };


        vk::Image glyphImage =mGContext_.getDevice().createImage(imageInfo);

        // 2. Allocate memory for the image, bind memory (you need to find memory type and allocate)
        vk::MemoryRequirements memRequirements = mGContext_.getDevice().getImageMemoryRequirements(glyphImage);

        vk::MemoryAllocateInfo allocInfo{
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = mGContext_.findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
        };

        vk::DeviceMemory imageMemory = mGContext_.getDevice().allocateMemory(allocInfo);

        mGContext_.getDevice().bindImageMemory(glyphImage, imageMemory, 0);

        // 3. Create vk::ImageView for the image
        vk::ImageViewCreateInfo viewInfo{
            .image = glyphImage,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR8Unorm,
            .subresourceRange = {
                .aspectMask =  vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        vk::ImageView glyphImageView = mGContext_.getDevice().createImageView(viewInfo);

        populateImage(glyphImage, bitmap, mGContext_);

        mCharacterImage_[c] = glyphImage;
        mCharacterMemory_[c] = imageMemory;
        mCharacterImageView_[c] = glyphImageView;
    }

    createPipeline(vertShader, fragShader, uniformBuffer);
}

// move constructor
Font::Font(Font&& other)
    : mGContext_(other.mGContext_) {

    mSampler_ = other.mSampler_;
    mCharacterFrontInfo_ = other.mCharacterFrontInfo_;
    mCharacterImage_ = other.mCharacterImage_;
    mCharacterMemory_ = other.mCharacterMemory_;
    mCharacterImageView_ = other.mCharacterImageView_;

    mPipeline_ = std::move(other.mPipeline_);
    mMaterial_ = std::move(other.mMaterial_);

    other.mSampler_ = nullptr;
    for (size_t i = 0; i < 128; ++i) {
        other.mCharacterImageView_[i] = nullptr;
        other.mCharacterImage_[i] = nullptr;
        other.mCharacterMemory_[i] = nullptr;
    }
}

// move assignment
Font& Font::operator=(Font&& other) noexcept {
    if (this != &other) {
        mSampler_ = other.mSampler_;
        mCharacterFrontInfo_ = other.mCharacterFrontInfo_;
        mCharacterImage_ = other.mCharacterImage_;
        mCharacterMemory_ = other.mCharacterMemory_;
        mCharacterImageView_ = other.mCharacterImageView_;

        mPipeline_ = std::move(other.mPipeline_);
        mMaterial_ = std::move(other.mMaterial_);

        other.mSampler_ = nullptr;
        for (size_t i = 0; i < 128; ++i) {
            other.mCharacterImageView_[i] = nullptr;
            other.mCharacterImage_[i] = nullptr;
            other.mCharacterMemory_[i] = nullptr;
        }
    }
    return *this;
}

Font::~Font() {
    for (size_t i = 0; i < 128; ++i) {
        if (mCharacterImageView_[i] != nullptr) {
            mGContext_.getDevice().destroyImageView(mCharacterImageView_[i]);
            mCharacterImageView_[i] = nullptr;
        }

        if (mCharacterImage_[i] != nullptr) {
            mGContext_.getDevice().destroyImage(mCharacterImage_[i]);
            mCharacterImage_[i] = nullptr;
        }

        if (mCharacterMemory_[i] != nullptr) {
            mGContext_.getDevice().freeMemory(mCharacterMemory_[i]);
            mCharacterMemory_[i] = nullptr;
        }
    }
    mGContext_.getDevice().destroySampler(mSampler_);
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

void Font::createPipeline(ShaderModule& vertShader, ShaderModule& fragShader, UniformBuffer& uniformBuffer) {
    clay::PipelineResource::PipelineConfig pipelineConfig{
        .graphicsContext = mGContext_
    };

    pipelineConfig.pipelineLayoutInfo.shaders = {
        &vertShader, &fragShader
    };

    auto vertexAttrib = Font::FontVertex::getAttributeDescriptions();
    pipelineConfig.pipelineLayoutInfo.attributeDescriptions = {vertexAttrib.begin(), vertexAttrib.end()};
    pipelineConfig.pipelineLayoutInfo.vertexInputBindingDescription = Font::FontVertex::getBindingDescription();

    pipelineConfig.pipelineLayoutInfo.depthStencilState = {
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False
    };

    pipelineConfig.pipelineLayoutInfo.rasterizerState = {
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f
    };

    pipelineConfig.pipelineLayoutInfo.pushConstants = {
        {
            .stageFlags = vk::ShaderStageFlagBits::eVertex |  vk::ShaderStageFlagBits::eFragment,
            .offset = 0,
            .size = sizeof(glm::mat4) + sizeof(glm::vec4)
        }
    };
    pipelineConfig.bindingLayoutInfo.bindings = {
        {
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 128,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
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
            .size = uniformBuffer.getSize(),
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer
        }
    };

    vk::SamplerCreateInfo samplerInfo{
        .magFilter = vk::Filter::eNearest,
        .minFilter = vk::Filter::eNearest,
        .mipmapMode = vk::SamplerMipmapMode::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToBorder,
        .addressModeV = vk::SamplerAddressMode::eClampToBorder,
        .addressModeW = vk::SamplerAddressMode::eClampToBorder,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::False,
        .maxAnisotropy = 1.0f,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eFloatTransparentBlack,
        .unnormalizedCoordinates = vk::False
    };


    mSampler_ = mGContext_.getDevice().createSampler(samplerInfo);

    for (int i = 0; i < 128; ++i) {
        matConfig.imageArrayBindings.push_back(
            {
                .sampler = mSampler_,
                .imageView = mCharacterImageView_[i] != nullptr ? mCharacterImageView_[i] : mCharacterImageView_['a'], // TODO fix this
                .binding = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler
            }
        );
    }

    mMaterial_ = std::make_unique<Material>(matConfig);
}

} // namespace clay