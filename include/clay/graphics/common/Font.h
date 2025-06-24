#pragma once
// standard lib
#include <unordered_map>
#include <array>
// third party
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/glm.hpp>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/utils/common/Utils.h"
#include "clay/graphics/common/PipelineResource.h"
#include "clay/graphics/common/Material.h"
#include "clay/graphics/common/UniformBuffer.h"

namespace clay {

class Font {
public:
    struct CharacterInfo {
        uint32_t width;        // glyph bitmap width in pixels
        uint32_t height;       // glyph bitmap height in pixels
        int32_t bitmapLeft;    // horizontal offset from pen position to left of glyph
        int32_t bitmapTop;     // vertical offset from pen baseline to top of glyph
        uint32_t advance;      // horizontal advance in pixels (usually in 1/64th pixels, convert as needed)
    };

    struct FontVertex {
        glm::vec4 vertex;    // xy = position, zw = texCoord
        int glyphIndex;

        static VkVertexInputBindingDescription getBindingDescription();

        static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
    };

    Font(BaseGraphicsContext& graphicsAPI, utils::FileData& fontFileData, ShaderModule& vertShader, ShaderModule& fragShader, UniformBuffer& uniformBuffer);

    const PipelineResource& getPipeline() const;

    const Material& getMaterial() const;

    ~Font();

    const CharacterInfo& getCharacterInfo(char c) const;

private:
    void createPipeline(ShaderModule& vertShader, ShaderModule& fragShader, UniformBuffer& uniformBuffer);

    BaseGraphicsContext& mGContext_;

    VkSampler mSampler_;

    std::array<CharacterInfo, 128> mCharacterFrontInfo_;

    std::array<VkImage, 128> mCharacterImage_;
    std::array<VkDeviceMemory, 128> mCharacterMemory_;
    std::array<VkImageView, 128> mCharacterImageView_;

    std::unique_ptr<PipelineResource> mPipeline_;
    std::unique_ptr<Material> mMaterial_;
};

} // namespace clay