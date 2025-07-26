#pragma once
// standard lib
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
// clay
#include "clay/audio/Audio.h"
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/Mesh.h"
#include "clay/graphics/common/Model.h"
#include "clay/graphics/common/Material.h"
#include "clay/graphics/common/Texture.h"
#include "clay/graphics/common/PipelineResource.h"
#include "clay/graphics/common/Font.h"
#include "clay/utils/common/Utils.h"

namespace clay {
class Resources {
public:
    template<typename T>
    struct Handle {
        uint32_t index = 0;
        uint32_t gen = 0;
    };

    template<typename T>
    class ResourcePool {
    public:
        ResourcePool(BaseGraphicsContext& graphicsContext);

        Handle<T> loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName);
        Handle<T> add(T&& obj, const std::string& name);
        void remove(Handle<T> handle);
        T& operator[](Handle<T> handle);
        Handle<T> getHandle(const std::string& name) const;

    private:
        BaseGraphicsContext& mGraphicsContext_;
        std::vector<T> resources;
        // track when a resource slot is resused, if a older generation is used, then throw error
        std::vector<uint32_t> generations; 
        // stack of vacant indices
        std::vector<uint32_t> freeList;
        // update when a resource is freed
        std::unordered_map<std::string, Handle<T>> name2Handle;
    };

    static void setFileLoader(std::function<utils::FileData(const std::string&)> loader);

    static void setResourcePath(const std::filesystem::path& path);

    static const std::filesystem::path& getResourcePath(); 

    Resources(BaseGraphicsContext& graphicsContext);

    ~Resources();

    template<typename T>
    Resources::Handle<T> loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName);

    template<typename T>
    auto addResource(T&& resource, const std::string& resourceName) -> Handle<std::remove_reference_t<T>>; 

    template<typename T>
    T& operator[](Handle<T> handle);

    template<typename T>
    Handle<T> getHandle(const std::string& resourceName);

    template<typename T>
    void release(Handle<T> handle);

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
};

} // namespace clay
