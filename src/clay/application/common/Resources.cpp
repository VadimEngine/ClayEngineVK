// class
#include "clay/application/common/Resources.h"

#define INSTANTIATE_RESOURCE_POOL(Type)                                         \
template class Resources::ResourcePool<Type>;                                   \
                                                                                \
template Resources::ResourcePool<Type>::ResourcePool(BaseGraphicsContext&);     \
                                                                                \
template Resources::Handle<Type>                                                \
    Resources::ResourcePool<Type>::loadResource(                                \
        const std::vector<std::string>&, const std::string&);                   \
                                                                                \
template Resources::Handle<Type>                                                \
    Resources::ResourcePool<Type>::add(Type&&, const std::string&);             \
                                                                                \
template void                                                                   \
    Resources::ResourcePool<Type>::remove(Resources::Handle<Type>);             \
                                                                                \
template Type&                                                                  \
    Resources::ResourcePool<Type>::operator[](Resources::Handle<Type>);         \
                                                                                \
template Resources::Handle<Type>                                                \
    Resources::ResourcePool<Type>::getHandle(const std::string&) const;         \
                                                                                \
template Resources::Handle<Type>                                                \
    Resources::loadResource<Type>(                                              \
        const std::vector<std::string>& resourcePath,                           \
        const std::string& resourceName                                         \
    );                                                                          \
                                                                                \
template auto                                                                   \
    Resources::addResource<Type>(                                               \
        Type&&,                                                                 \
        const std::string&                                                      \
    ) -> Handle<std::remove_reference_t<Type>>;                                 \
                                                                                \
template Type& Resources::operator[](Handle<Type> handle);                      \
                                                                                \
template Resources::Handle<Type>                                                \
    Resources::getHandle(const std::string& resourceName);                      \
                                                                                \
template void Resources::release<Type>(Handle<Type> handle);                     


namespace clay {

void Resources::setFileLoader(std::function<utils::FileData(const std::string&)> loader) {
    loadFileToMemory = std::move(loader);
}

void Resources::setResourcePath(const std::filesystem::path& path) {
    RESOURCE_PATH = path;
}

const std::filesystem::path& Resources::getResourcePath() {
    return RESOURCE_PATH;
}

std::filesystem::path Resources::RESOURCE_PATH = "";

std::function<utils::FileData(const std::string&)> Resources::loadFileToMemory;

// START ResourcePool

template<typename T>
Resources::ResourcePool<T>::ResourcePool(BaseGraphicsContext& graphicsContext)
    : mGraphicsContext_(graphicsContext) {}

template<typename T>
Resources::Handle<T> Resources::ResourcePool<T>::loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        utils::FileData loadedFile = loadFileToMemory(resourcePaths[0]);
        std::vector<clay::Mesh> loadedMeshes;
        Mesh::parseObjFile(mGraphicsContext_, loadedFile, loadedMeshes);
        // TODO confirm there is only 1 mesh here // TODO mesh clean up seems wrong. There is delete happening due to the std::vector<clay::Mesh> being passed?
        return add(std::move(loadedMeshes[0]), resourceName);
    } else if constexpr(std::is_same_v<T, VkSampler>) {
        throw std::runtime_error("Load not implemented for VkSampler");
    } else if constexpr(std::is_same_v<T, Model>) {
        throw std::runtime_error("Load not implemented for Model");
    } else if constexpr(std::is_same_v<T, Texture>) {
        throw std::runtime_error("Load not implemented for Texture");
    } else if constexpr(std::is_same_v<T, PipelineResource>) {
        throw std::runtime_error("Load not implemented for PipelineResource");
    } else if constexpr(std::is_same_v<T, Material>) {
        throw std::runtime_error("Load not implemented for Material");
    } else if constexpr (std::is_same_v<T, Audio>) {
        //utils::FileData loadedFile = loadFileToMemory(resourcePaths[0]);
        //mAudios_[resourceName] = std::make_unique<Audio>(loadedFile);
        throw std::runtime_error("Load not implemented for Audio");
    }
    else if constexpr (std::is_same_v<T, Font>) {
        throw std::runtime_error("Load not implemented for Font");
    } else {
        throw std::runtime_error("Load not implemented.");
    }
}

template<typename T>
Resources::Handle<T> Resources::ResourcePool<T>::add(T&& obj, const std::string& name) {
    uint32_t idx;
    if (!freeList.empty()) {
        idx = freeList.back();
        freeList.pop_back();
        resources[idx] = std::move(obj);
    } else {
        idx = static_cast<uint32_t>(resources.size());
        resources.emplace_back(std::move(obj));
        generations.push_back(0);
    }
    name2Handle[name] = Handle<T>{ idx, generations[idx] };
    return Handle<T>{ idx, generations[idx] };
}

template<typename T>
void Resources::ResourcePool<T>::remove(Handle<T> handle) {
    if (handle.index >= resources.size()) return; 
    if (generations[handle.index] != handle.gen) return;

    // Destroy the object explicitly
    resources[handle.index].~T(); // or std::destroy_at()

    // Mark slot free
    generations[handle.index]++;
    freeList.push_back(handle.index);
}

template<typename T>
T& Resources::ResourcePool<T>::operator[](Handle<T> handle) {
    assert(handle.index < resources.size());
    assert(generations[handle.index] == handle.gen);
    return resources[handle.index];
}

template<typename T>
Resources::Handle<T> Resources::ResourcePool<T>::getHandle(const std::string& name) const {
    auto it = name2Handle.find(name);
    if (it == name2Handle.end()) {
        throw std::runtime_error("Unknown resource: " + name);
    }
    return it->second;
}

// END ResourcePool

// START Resources

Resources::Resources(BaseGraphicsContext& graphicsContext) :
    mGraphicsContext_(graphicsContext),
    mMeshesPool_(mGraphicsContext_),
    mModelsPool_(mGraphicsContext_),
    mSamplersPool_(mGraphicsContext_),
    mTexturesPool_(mGraphicsContext_),
    mPipePool_(mGraphicsContext_),
    mMaterialsPool_(mGraphicsContext_),
    mAudiosPool_(mGraphicsContext_),
    mFontsPool_(mGraphicsContext_) {};

Resources::~Resources() {
    releaseAll();
}

template<typename T>
Resources::Handle<T> Resources::loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        return mMeshesPool_.loadResource(resourcePaths, resourceName);
    } else if constexpr(std::is_same_v<T, VkSampler>) {
        throw std::runtime_error("Load not implemented for VkSampler");
    } else if constexpr(std::is_same_v<T, Model>) {
        throw std::runtime_error("Load not implemented for Model");
    } else if constexpr(std::is_same_v<T, Texture>) {
        throw std::runtime_error("Load not implemented for Texture");
    } else if constexpr(std::is_same_v<T, PipelineResource>) {
        throw std::runtime_error("Load not implemented for PipelineResource");
    } else if constexpr(std::is_same_v<T, Material>) {
        throw std::runtime_error("Load not implemented for Material");
    } else if constexpr (std::is_same_v<T, Audio>) {
        //utils::FileData loadedFile = loadFileToMemory(resourcePaths[0]);
        //mAudios_[resourceName] = std::make_unique<Audio>(loadedFile);
        throw std::runtime_error("Load not implemented for Audio");
    }
    else if constexpr (std::is_same_v<T, Font>) {
        throw std::runtime_error("Load not implemented for Font");
    } else {
        throw std::runtime_error("Load not implemented.");
    }
}

template<typename T>
auto Resources::addResource(T&& resource, const std::string& resourceName) -> Handle<std::remove_reference_t<T>> {
    using U = std::remove_reference_t<T>;

    if constexpr (std::is_same_v<U, Mesh>)
        return mMeshesPool_.add(std::forward<T>(resource), resourceName);

    else if constexpr (std::is_same_v<U, Model>)
        return mModelsPool_.add(std::forward<T>(resource), resourceName);

    else if constexpr (std::is_same_v<U, VkSampler>)
        return mSamplersPool_.add(std::forward<T>(resource), resourceName);

    else if constexpr (std::is_same_v<U, Texture>)
        return mTexturesPool_.add(std::forward<T>(resource), resourceName);

    else if constexpr (std::is_same_v<U, PipelineResource>)
        return mPipePool_.add(std::forward<T>(resource), resourceName);

    else if constexpr (std::is_same_v<U, Material>)
        return mMaterialsPool_.add(std::forward<T>(resource), resourceName);

    else if constexpr (std::is_same_v<U, Audio>)
        return mAudiosPool_.add(std::forward<T>(resource), resourceName);

    else if constexpr (std::is_same_v<U, Font>)
        return mFontsPool_.add(std::forward<T>(resource), resourceName);

    else
        static_assert(std::is_same_v<U, void>,
                      "addResource: unsupported resource type");
}

template<typename T>
T& Resources::operator[](Handle<T> handle) {
    if constexpr (std::is_same_v<T, Mesh>) {
        return mMeshesPool_[handle];
    } else if constexpr (std::is_same_v<T, Model>) {
        return mModelsPool_[handle];
    } else if constexpr (std::is_same_v<T, VkSampler>) {
        return mSamplersPool_[handle];
    } else if constexpr (std::is_same_v<T, Texture>) {
        return mTexturesPool_[handle];
    } else if constexpr (std::is_same_v<T, PipelineResource>) {
        return mPipePool_[handle];
    } else if constexpr (std::is_same_v<T, Material>) {
        return mMaterialsPool_[handle];
    } else if constexpr (std::is_same_v<T, Audio>) {
        return mAudiosPool_[handle];
    } else if constexpr (std::is_same_v<T, Font>) {
        return mFontsPool_[handle];
    }
}

template<typename T>
Resources::Handle<T> Resources::getHandle(const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        return mMeshesPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, Model>) {
        return mModelsPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, VkSampler>) {
        return mSamplersPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, Texture>) {
        return mTexturesPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, PipelineResource>) {
        return mPipePool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, Material>) {
        return mMaterialsPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, Audio>) {
        return mAudiosPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, Font>) {
        return mFontsPool_.getHandle(resourceName);
    }
}

template<typename T>
void Resources::release(Handle<T> handle) {
    // if constexpr (std::is_same_v<T, Mesh>) {
    //     // auto it = mMeshes_.find(resourceName);
    //     // if (it != mMeshes_.end()) {
    //     //     mMeshes_.erase(it);
    //     // }
    //     mMeshesPool_.remove(handle);
    // } else if constexpr (std::is_same_v<T, Model>) {
    //     auto it = mModels_.find(resourceName);
    //     if (it != mModels_.end()) {
    //         mModels_.erase(it);
    //     }
    // } else if constexpr(std::is_same_v<T, VkSampler>) {
    //     auto it = mSamplers_.find(resourceName);
    //     if (it != mSamplers_.end()) {
    //         mSamplers_.erase(it);
    //     }
    // } else if constexpr(std::is_same_v<T, Texture>) {
    //     auto it = mTextures_.find(resourceName);
    //     if (it != mTextures_.end()) {
    //         mTextures_.erase(it);
    //     }
    // } else if constexpr(std::is_same_v<T, PipelineResource>) {
    //     auto it = mPipelineResources_.find(resourceName);
    //     if (it != mPipelineResources_.end()) {
    //         mPipelineResources_.erase(it);
    //     }
    // } else if constexpr(std::is_same_v<T, Material>) {
    //     auto it = mMaterials_.find(resourceName);
    //     if (it != mMaterials_.end()) {
    //         mMaterials_.erase(it);
    //     }
    // } else if constexpr (std::is_same_v<T, Audio>) {
    //     auto it = mAudios_.find(resourceName);
    //     if (it != mAudios_.end()) {
    //         mAudios_.erase(it);
    //     }
    // }
    // else if constexpr (std::is_same_v<T, Font>) {
    //     auto it = mFonts_.find(resourceName);
    //     if (it != mFonts_.end()) {
    //         mFonts_.erase(it);
    //     }
    // }
}

void Resources::releaseAll() {
    // mMeshes_.clear();
    // mModels_.clear();
    // for (auto& [_, eachSampler]: mSamplers_) {
    //     vkDestroySampler(mGraphicsContext_.getDevice(), *eachSampler, nullptr);
    // }
    // mSamplers_.clear(); // free all
    // mTextures_.clear();
    // mPipelineResources_.clear();
    // mMaterials_.clear();
    // mAudios_.clear();
    // mFonts_.clear();
}

// END Resources

// Explicitly instantiate templates for expected types
INSTANTIATE_RESOURCE_POOL(Mesh)
INSTANTIATE_RESOURCE_POOL(Model)
INSTANTIATE_RESOURCE_POOL(VkSampler)
INSTANTIATE_RESOURCE_POOL(Texture)
INSTANTIATE_RESOURCE_POOL(PipelineResource)
INSTANTIATE_RESOURCE_POOL(Material)
INSTANTIATE_RESOURCE_POOL(Audio)
INSTANTIATE_RESOURCE_POOL(Font)

} // namespace clay
