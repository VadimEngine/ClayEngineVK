#pragma once
// standard lib
#include <unordered_map>
#include <array>
// clay
#include "clay/graphics/common/IGraphicsContext.h"
#include "clay/utils/common/Utils.h"
#include "clay/graphics/common/PipelineResource.h"
#include "clay/graphics/common/Material.h"

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

    struct CameraConstant {
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct FontVertex {
        glm::vec4 vertex;    // xy = position, zw = texCoord
        int glyphIndex;

        static VkVertexInputBindingDescription getBindingDescription();

        static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
    };


    Font(IGraphicsContext& graphicsAPI, utils::FileData& fileData, utils::FileData& vertexFileData, utils::FileData& fragmentFileData);

    ~Font();

    void createPipeline(utils::FileData& vertexFileData, utils::FileData& fragmentFileData);

    void createVertexBuffer();

    void initTemp(const std::string& str);

    void render(VkCommandBuffer cmdBuffer);

    glm::mat4 getModelMatrix();

    IGraphicsContext& mGContext_;

    std::array<CharacterInfo, 128> mCharacterFrontInfo_;

    std::array<VkImage, 128> mCharacterImage_;
    std::array<VkDeviceMemory, 128> mCharacterMemory_;
    std::array<VkImageView, 128> mCharacterImageView_;

    PipelineResource* mPipeline_;
    Material* mMaterial_;

    VkBuffer mVertexBuffer_;
    VkDeviceMemory mVertexBufferMemory_;
    std::vector<FontVertex> mVertices_;

    glm::vec3 mPosition_ = {0.0f, 0.0f, 0.0f};
    glm::vec3 mRotation_ = { 0.0f, 0.0f, 0.0f };
    glm::vec3 mScale_ = { 1.0f, 1.0f, 1.0f };

    std::string mString_;


};

} // namespace clay