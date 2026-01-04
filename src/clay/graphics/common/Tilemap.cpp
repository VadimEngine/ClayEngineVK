// clay
#include "clay/utils/common/Logger.h"
#include "clay/utils/desktop/UtilsDesktop.h"
#include "clay/graphics/common/Tilemap.h"
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

Tilemap::Tilemap(clay::ecs::EntityManager& entityManager, clay::Resources& resources,
                 clay::Handle<Material> materialHandle, clay::Handle<Mesh> meshHandle)
    : mEntityManager_(entityManager)
    , mResources_(resources)
    , mGraphicsContext_(entityManager.mRenderSystem_.mGContext_)
    , mMaterialHandle_(materialHandle)
    , mMeshHandle_(meshHandle) {
    // Register with entity manager
    mEntityManager_.addTilemap(this);
}

Tilemap::~Tilemap() {
    // Unregister from entity manager
    mEntityManager_.removeTilemap(this);
    destroyInstanceBuffer();
}

void Tilemap::registerTile(int tileId, const TileDefinition& def) {
    mTileDefinitions_[tileId] = def;
}

void Tilemap::load(const std::vector<std::vector<int>>& tileIds) {
    if (tileIds.empty() || tileIds[0].empty()) {
        LOG_W("Tilemap::load - empty tile data");
        return;
    }

    mMapHeight_ = static_cast<int>(tileIds.size());
    mMapWidth_ = static_cast<int>(tileIds[0].size());
    
    // Initialize tilemap
    mTilemap_.resize(mMapHeight_);
    for (int y = 0; y < mMapHeight_; y++) {
        mTilemap_[y].resize(mMapWidth_);
    }
    
    buildInstanceData(tileIds);
}

const Tilemap::Tile& Tilemap::getTile(int x, int y) const {
    static Tile emptyTile;
    if (x < 0 || x >= mMapWidth_ || y < 0 || y >= mMapHeight_) {
        return emptyTile;
    }
    return mTilemap_[y][x];
}

void Tilemap::buildInstanceData(const std::vector<std::vector<int>>& tileIds) {
    mInstanceData_.clear();
    
    // Build instance data for all tiles
    for (int y = 0; y < mMapHeight_; y++) {
        for (int x = 0; x < mMapWidth_; x++) {
            int tileId = tileIds[y][x];
            
            // Skip tiles with ID -1 (empty)
            if (tileId < 0) {
                continue;
            }
            
            // Check if tile definition exists
            auto it = mTileDefinitions_.find(tileId);
            if (it == mTileDefinitions_.end()) {
                LOG_W("Tilemap::load - no definition for tile ID %d at (%d, %d)", tileId, x, y);
                continue;
            }
            
            const TileDefinition& tileDef = it->second;
            
            TileInstanceData instance;
            instance.position = glm::vec4(
                (x - mMapWidth_ / 2.0f) * mTileSize_,
                (y - mMapHeight_ / 2.0f) * mTileSize_,
                -1.0f,  // z-layer for tiles
                mTileSize_  // w = scale
            );
            instance.uvOffsets = tileDef.uvCoords;
            instance.color = tileDef.color;
            
            mInstanceData_.push_back(instance);
            
            // Store tile info
            mTilemap_[y][x].tileId = tileId;
            mTilemap_[y][x].entity = 0; // No entity when using instancing
        }
    }
    
    mTileCount_ = static_cast<uint32_t>(mInstanceData_.size());
    
    // Create/update instance buffer (SSBO)
    if (mTileCount_ > 0) {
        createInstanceBuffer();
    }
}

void Tilemap::createInstanceBuffer() {
    // Destroy old buffer if exists
    destroyInstanceBuffer();
    
    if (mInstanceData_.empty()) {
        return;
    }
    
    vk::DeviceSize bufferSize = sizeof(TileInstanceData) * mInstanceData_.size();
    
    // Create staging buffer
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    mGraphicsContext_.createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );
    
    // Copy data to staging buffer
    void* data = mGraphicsContext_.getDevice().mapMemory(stagingBufferMemory, 0, bufferSize);
    std::memcpy(data, mInstanceData_.data(), static_cast<size_t>(bufferSize));
    mGraphicsContext_.getDevice().unmapMemory(stagingBufferMemory);
    
    // Create device-local SSBO
    mGraphicsContext_.createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mInstanceBuffer_,
        mInstanceBufferMemory_
    );
    
    // Copy from staging to device buffer
    mGraphicsContext_.copyBuffer(stagingBuffer, mInstanceBuffer_, bufferSize);
    
    // Clean up staging buffer
    mGraphicsContext_.getDevice().destroyBuffer(stagingBuffer);
    mGraphicsContext_.getDevice().freeMemory(stagingBufferMemory);
    
    // Update material descriptor set to include SSBO
    updateMaterialDescriptorSet(bufferSize);
}

void Tilemap::destroyInstanceBuffer() {
    if (mInstanceBuffer_) {
        mGraphicsContext_.getDevice().destroyBuffer(mInstanceBuffer_);
        mInstanceBuffer_ = nullptr;
    }
    if (mInstanceBufferMemory_) {
        mGraphicsContext_.getDevice().freeMemory(mInstanceBufferMemory_);
        mInstanceBufferMemory_ = nullptr;
    }
}

void Tilemap::updateMaterialDescriptorSet(vk::DeviceSize bufferSize) {
    // Update the material's descriptor set to include the SSBO at binding 2
    Material& material = mResources_[mMaterialHandle_];
    
    vk::DescriptorBufferInfo bufferInfo{
        .buffer = mInstanceBuffer_,
        .offset = 0,
        .range = bufferSize
    };
    
    vk::WriteDescriptorSet descriptorWrite{
        .dstSet = material.getDescriptorSet(),
        .dstBinding = 2,  // SSBO binding point
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo = &bufferInfo
    };
    
    mGraphicsContext_.getDevice().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
}

} // namespace clay
