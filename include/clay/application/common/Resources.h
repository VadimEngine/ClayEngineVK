#pragma once
// standard lib
#include <filesystem>
#include <functional>
#include <memory>
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

/*
 TODO maybe do a data oriented design where containers are std::vector<T> rather than map<T*>
 - getResources will return a reference accessed by index. Need to add a namedToIndex()
 - when releasing resources, keep track of available slots to use when adding a new resource
 - maybe have external and internal id where external is what outside classes use while internal is the index
   in the container (which updates when data is moved around) name->internalId->externalId
 */

namespace clay {
class Resources {
public:

    static void setFileLoader(std::function<utils::FileData(const std::string&)> loader);

    static void setResourcePath(const std::filesystem::path& path);

    static const std::filesystem::path& getResourcePath(); 

    Resources(BaseGraphicsContext& graphicsContext);

    ~Resources();

    template<typename T>
    void loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName);

    // TODO take in a raw pointer to make calling this method more simple
    template<typename T>
    void addResource(std::unique_ptr<T> resource, const std::string& resourceName);

    template<typename T>
    T* getResource(const std::string& resourceName);

    template<typename T>
    void release(const std::string& resourceName);

    void releaseAll();

    ////////////
    // TODO use a ResourcePool Struct instead
    template<typename T>
    struct Handle {
        uint32_t index   = 0;
        uint32_t gen     = 0;
    };

    template<typename T>
    struct ResourcePool {

        std::vector<T> resources;
        std::vector<uint32_t> generations; // track when a resource slot is resused, if a older generation is used, then throw error
        std::vector<uint32_t> freeList;      // stack of vacant indices
        std::unordered_map<std::string, Handle<T>> name2Handle;

        Handle<T> loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName);
        Handle<T> add(const std::string& name, T&& obj);
        void remove(Handle<T> handle);
        T& operator[](Handle<T> handle);
        Handle<T> getHandle(const std::string& name) const;
    };

    ResourcePool<Mesh> mMeshesPool_;
    ResourcePool<Model> mModelsPool_;
    ResourcePool<VkSampler> mSamplersPool_;
    ResourcePool<Texture> mTexturesPool_;
    ResourcePool<PipelineResource> mPipePool_;
    ResourcePool<Material> mMaterialsPool_;
    ResourcePool<Audio> mAudiosPool_;
    ResourcePool<Font> mFontsPool_;

    //template<typename T>
    //uint32_t addResource2(const std::string& name, T&& resource);

    //template<typename T>
    //T& getResource2(uint32_t handle);

    //template<typename T>
    //T& getResource2(const std::string& resourceName);

    //std::unordered_map<std::string, uint32_t> 
    //std::vector<Model> mModels2_;
////////////////////

private:
    // TODO maybe this can be class member. If for some reason resources are location specific
    static std::filesystem::path RESOURCE_PATH;

    static std::function<utils::FileData(const std::string&)> loadFileToMemory;

    BaseGraphicsContext& mGraphicsContext_;

    // TODO shaders
    std::unordered_map<std::string, std::unique_ptr<Mesh>> mMeshes_;
    std::unordered_map<std::string, std::unique_ptr<Model>> mModels_;
    std::unordered_map<std::string, std::unique_ptr<VkSampler>> mSamplers_;
    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures_;
    std::unordered_map<std::string, std::unique_ptr<PipelineResource>> mPipelineResources_;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials_;
    std::unordered_map<std::string, std::unique_ptr<Audio>> mAudios_;
    std::unordered_map<std::string, std::unique_ptr<Font>> mFonts_;



};

} // namespace clay
