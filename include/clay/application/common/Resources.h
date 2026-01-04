#pragma once
// standard lib
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
// clay
#include "clay/application/common/Handle.h"
#include "clay/audio/Audio.h"
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/graphics/common/Model.h"
#include "clay/graphics/common/Material.h"
#include "clay/graphics/common/Texture.h"
#include "clay/graphics/common/PipelineResource.h"
#include "clay/graphics/common/Font.h"
#include "clay/graphics/common/AnimatedMesh.h"
#include "clay/graphics/common/SkeletalAnimation.h"
#include "clay/graphics/common/Animation2D.h"
#include "clay/utils/common/Utils.h"

namespace clay {

class Resources {
public:
    template<typename T>
    class ResourcePool {
    public:
        ResourcePool(BaseGraphicsContext& graphicsContext);

        clay::Handle<T> loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName);
        clay::Handle<T> add(T&& obj, const std::string& name);
        void remove(clay::Handle<T> handle);
        void clear();
        T& operator[](clay::Handle<T> handle);
        clay::Handle<T> getHandle(const std::string& name) const;

    private:
        BaseGraphicsContext& mGraphicsContext_;
        std::vector<T> resources;
        // track when a resource slot is resused, if a older generation is used, then throw error
        std::vector<uint32_t> generations; 
        // stack of vacant indices
        std::vector<uint32_t> freeList;
        // update when a resource is freed
        std::unordered_map<std::string, clay::Handle<T>> name2Handle;
    };

    static void setFileLoader(std::function<utils::FileData(const std::string&)> loader);

    static void setResourcePath(const std::filesystem::path& path);

    static const std::filesystem::path& getResourcePath(); 

    Resources(BaseGraphicsContext& graphicsContext);

    ~Resources();

    template<typename T>
    clay::Handle<T> loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName);

    template<typename T>
    auto addResource(T&& resource, const std::string& resourceName) -> clay::Handle<std::remove_reference_t<T>>; 

    template<typename T>
    T& operator[](clay::Handle<T> handle);

    template<typename T>
    clay::Handle<T> getHandle(const std::string& resourceName);

    template<typename T>
    void release(clay::Handle<T> handle);

    void releaseAll();

private:
    static std::filesystem::path RESOURCE_PATH;

    static std::function<utils::FileData(const std::string&)> loadFileToMemory;

    BaseGraphicsContext& mGraphicsContext_;

    ResourcePool<Mesh> mMeshesPool_;
    ResourcePool<Model> mModelsPool_;
    ResourcePool<vk::Sampler> mSamplersPool_;
    ResourcePool<Texture> mTexturesPool_;
    ResourcePool<PipelineResource> mPipePool_;
    ResourcePool<Material> mMaterialsPool_;
    ResourcePool<Audio> mAudiosPool_;
    ResourcePool<Font> mFontsPool_;
    ResourcePool<Animation2D> mAnimations2DPool_;
    ResourcePool<AnimatedMesh> mAnimatedMeshesPool_;
    ResourcePool<SkeletalAnimation> mSkeletalAnimationsPool_;
};

} // namespace clay
