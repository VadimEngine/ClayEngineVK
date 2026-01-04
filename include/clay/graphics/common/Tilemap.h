#pragma once
// standard lib
#include <vector>
#include <string>
#include <unordered_map>
// third party
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
// clay
#include "clay/ecs/EntityManager.h"
#include "clay/application/common/Resources.h"

namespace clay {

struct TileDefinition {
    glm::vec4 uvCoords;           // Sprite offset in spritesheet
    glm::vec4 color = {1,1,1,1};  // Tint color
};

class Tilemap {
public:
    struct Tile {
        int tileId = -1;
        clay::ecs::Entity entity = 0;
    };

    Tilemap(clay::ecs::EntityManager& entityManager, clay::Resources& resources,
            clay::Handle<Material> materialHandle, clay::Handle<Mesh> meshHandle);
    ~Tilemap();

    // Register a tile type definition
    void registerTile(int tileId, const TileDefinition& def);
    
    // Load tilemap from 2D array of tile IDs
    void load(const std::vector<std::vector<int>>& tileIds);
    
    int getWidth() const { return mMapWidth_; }
    int getHeight() const { return mMapHeight_; }
    float getTileSize() const { return mTileSize_; }
    void setTileSize(float size) { mTileSize_ = size; }
    
    const Tile& getTile(int x, int y) const;
    const std::vector<std::vector<Tile>>& getTilemap() const { return mTilemap_; }
    
    // Accessors for rendering (used by RenderSystem)
    clay::Handle<Material> getMaterialHandle() const { return mMaterialHandle_; }
    clay::Handle<Mesh> getMeshHandle() const { return mMeshHandle_; }
    uint32_t getTileCount() const { return mTileCount_; }

private:
    // Instance data structure matching the shader SSBO layout
    struct TileInstanceData {
        glm::vec4 position;   // xyz = world position, w = scale
        glm::vec4 uvOffsets;  // x,y = UV offset, z,w = UV size  
        glm::vec4 color;      // rgba tint
    };
    
    clay::ecs::EntityManager& mEntityManager_;
    clay::Resources& mResources_;
    BaseGraphicsContext& mGraphicsContext_;
    
    std::unordered_map<int, TileDefinition> mTileDefinitions_;
    clay::Handle<Material> mMaterialHandle_;
    clay::Handle<Mesh> mMeshHandle_;
    
    std::vector<std::vector<Tile>> mTilemap_;
    int mMapWidth_ = 0;
    int mMapHeight_ = 0;
    float mTileSize_ = 0.2f;
    
    // Instance rendering data
    std::vector<TileInstanceData> mInstanceData_;
    vk::Buffer mInstanceBuffer_;
    vk::DeviceMemory mInstanceBufferMemory_;
    uint32_t mTileCount_ = 0;
    
    // Helper methods
    void createInstanceBuffer();
    void destroyInstanceBuffer();
    void updateMaterialDescriptorSet(vk::DeviceSize bufferSize);
    void buildInstanceData(const std::vector<std::vector<int>>& tileIds);
};

} // namespace clay
