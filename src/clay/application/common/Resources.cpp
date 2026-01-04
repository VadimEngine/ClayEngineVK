// class
#include "clay/application/common/Resources.h"

#define INSTANTIATE_RESOURCE_POOL(Type)                                         \
template class Resources::ResourcePool<Type>;                                   \
                                                                                \
template clay::Handle<Type>                                                     \
    Resources::loadResource<Type>(                                              \
        const std::vector<std::string>& resourcePath,                           \
        const std::string& resourceName                                         \
    );                                                                          \
                                                                                \
template auto                                                                   \
    Resources::addResource<Type>(                                               \
        Type&&,                                                                 \
        const std::string&                                                      \
    ) -> clay::Handle<std::remove_reference_t<Type>>;                           \
                                                                                \
template Type& Resources::operator[](clay::Handle<Type> handle);                \
                                                                                \
template clay::Handle<Type>                                                     \
    Resources::getHandle(const std::string& resourceName);                      \
                                                                                \
template void Resources::release<Type>(clay::Handle<Type> handle);


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
clay::Handle<T> Resources::ResourcePool<T>::loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        utils::FileData loadedFile = loadFileToMemory(resourcePaths[0]);
        std::vector<clay::Mesh> loadedMeshes;
        Mesh::parseObjFile(mGraphicsContext_, loadedFile, loadedMeshes);
        // TODO confirm there is only 1 mesh here // TODO mesh clean up seems wrong. There is delete happening due to the std::vector<clay::Mesh> being passed?
        return add(std::move(loadedMeshes[0]), resourceName);
    } else if constexpr(std::is_same_v<T, vk::Sampler>) {
        throw std::runtime_error("Load not implemented for vk::Sampler");
    } else if constexpr(std::is_same_v<T, Model>) {
        throw std::runtime_error("Load not implemented for Model");
    } else if constexpr(std::is_same_v<T, Texture>) {
        throw std::runtime_error("Load not implemented for Texture");
    } else if constexpr(std::is_same_v<T, PipelineResource>) {
        throw std::runtime_error("Load not implemented for PipelineResource");
    } else if constexpr(std::is_same_v<T, Material>) {
        throw std::runtime_error("Load not implemented for Material");
    } else if constexpr (std::is_same_v<T, Audio>) {
        utils::FileData loadedFile = loadFileToMemory(resourcePaths[0]);
        return add(Audio(loadedFile), resourceName);
        throw std::runtime_error("Load not implemented for Audio");
    }
    else if constexpr (std::is_same_v<T, Font>) {
        throw std::runtime_error("Load not implemented for Font");
    } else {
        throw std::runtime_error("Load not implemented.");
    }
}

template<typename T>
clay::Handle<T> Resources::ResourcePool<T>::add(T&& obj, const std::string& name) {
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
void Resources::ResourcePool<T>::clear() {
    // For samplers, we need to manually destroy them before clearing
    if constexpr (std::is_same_v<T, vk::Sampler>) {
        for (auto& sampler : resources) {
            if (sampler) {
                mGraphicsContext_.getDevice().destroySampler(sampler, nullptr);
            }
        }
    }
    // Clear all resources - this calls destructors for Mesh, Texture, etc.
    resources.clear();
    generations.clear();
    freeList.clear();
    name2Handle.clear();
}

template<typename T>
T& Resources::ResourcePool<T>::operator[](Handle<T> handle) {
    assert(handle.index < resources.size());
    assert(generations[handle.index] == handle.gen);
    return resources[handle.index];
}

template<typename T>
clay::Handle<T> Resources::ResourcePool<T>::getHandle(const std::string& name) const {
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
    mFontsPool_(mGraphicsContext_),
    mAnimations2DPool_(mGraphicsContext_),
    mAnimatedMeshesPool_(mGraphicsContext_),
    mSkeletalAnimationsPool_(mGraphicsContext_) {};

Resources::~Resources() {
    releaseAll();
}

template<typename T>
clay::Handle<T> Resources::loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        return mMeshesPool_.loadResource(resourcePaths, resourceName);
    } else if constexpr(std::is_same_v<T, vk::Sampler>) {
        throw std::runtime_error("Load not implemented for vk::Sampler");
    } else if constexpr(std::is_same_v<T, Model>) {
        throw std::runtime_error("Load not implemented for Model");
    } else if constexpr(std::is_same_v<T, Texture>) {
        throw std::runtime_error("Load not implemented for Texture");
    } else if constexpr(std::is_same_v<T, PipelineResource>) {
        throw std::runtime_error("Load not implemented for PipelineResource");
    } else if constexpr(std::is_same_v<T, Material>) {
        throw std::runtime_error("Load not implemented for Material");
    } else if constexpr (std::is_same_v<T, Audio>) {
        return mAudiosPool_.loadResource(resourcePaths, resourceName);
    } else if constexpr (std::is_same_v<T, Font>) {
        throw std::runtime_error("Load not implemented for Font");
    } else {
        throw std::runtime_error(std::string("Load not implemented for type: ") + typeid(T).name());
    }
}

template<typename T>
auto Resources::addResource(T&& resource, const std::string& resourceName) -> Handle<std::remove_reference_t<T>> {
    using U = std::remove_reference_t<T>;

    if constexpr (std::is_same_v<U, Mesh>) {
        return mMeshesPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, Model>) {
        return mModelsPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, vk::Sampler>) {
        return mSamplersPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, Texture>) {
        return mTexturesPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, PipelineResource>) {
        return mPipePool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, Material>) {
        return mMaterialsPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, Audio>) {
        return mAudiosPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, Font>) {
        return mFontsPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, Animation2D>) {
        return mAnimations2DPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, AnimatedMesh>) {
        return mAnimatedMeshesPool_.add(std::forward<T>(resource), resourceName);
    } else if constexpr (std::is_same_v<U, SkeletalAnimation>) {
        return mSkeletalAnimationsPool_.add(std::forward<T>(resource), resourceName);
    } else {
        throw std::runtime_error(std::string("addResource: unsupported resource type: ") + typeid(T).name());
    }
}

template<typename T>
T& Resources::operator[](Handle<T> handle) {
    if constexpr (std::is_same_v<T, Mesh>) {
        return mMeshesPool_[handle];
    } else if constexpr (std::is_same_v<T, Model>) {
        return mModelsPool_[handle];
    } else if constexpr (std::is_same_v<T, vk::Sampler>) {
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
    } else if constexpr (std::is_same_v<T, Animation2D>) {
        return mAnimations2DPool_[handle];
    } else if constexpr (std::is_same_v<T, AnimatedMesh>) {
        return mAnimatedMeshesPool_[handle];
    } else if constexpr (std::is_same_v<T, SkeletalAnimation>) {
        return mSkeletalAnimationsPool_[handle];
    } else {
        throw std::runtime_error(std::string("operator[]: unsupported resource type: ") + typeid(T).name());
    }
}

template<typename T>
clay::Handle<T> Resources::getHandle(const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        return mMeshesPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, Model>) {
        return mModelsPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, vk::Sampler>) {
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
    } else if constexpr (std::is_same_v<T, Animation2D>) {
        return mAnimations2DPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, AnimatedMesh>) {
        return mAnimatedMeshesPool_.getHandle(resourceName);
    } else if constexpr (std::is_same_v<T, SkeletalAnimation>) {
        return mSkeletalAnimationsPool_.getHandle(resourceName);
    } else {
        throw std::runtime_error(std::string("getHandle: unsupported resource type: ") + typeid(T).name());
    }
}

template<typename T>
void Resources::release(Handle<T> handle) {
    if constexpr (std::is_same_v<T, Mesh>) {
        mMeshesPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, Model>) {
        mModelsPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, vk::Sampler>) {
        mSamplersPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, Texture>) {
        mTexturesPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, PipelineResource>) {
        mPipePool_.remove(handle);
    } else if constexpr (std::is_same_v<T, Material>) {
        mMaterialsPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, Audio>) {
        mAudiosPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, Font>) {
        mFontsPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, Animation2D>) {
        mAnimations2DPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, AnimatedMesh>) {
        mAnimatedMeshesPool_.remove(handle);
    } else if constexpr (std::is_same_v<T, SkeletalAnimation>) {
        mSkeletalAnimationsPool_.remove(handle);
    } else {
        throw std::runtime_error(std::string("release: unsupported resource type: ") + typeid(T).name());
    }
}

void Resources::releaseAll() {
    // Clear all resource pools in proper order
    mMeshesPool_.clear();
    mModelsPool_.clear();
    mSamplersPool_.clear();
    mTexturesPool_.clear();
    mPipePool_.clear();
    mMaterialsPool_.clear();
    mAudiosPool_.clear();
    mFontsPool_.clear();
    mAnimations2DPool_.clear();
    mAnimatedMeshesPool_.clear();
    mSkeletalAnimationsPool_.clear();
}

// END Resources

// Explicitly instantiate templates for expected types
INSTANTIATE_RESOURCE_POOL(Mesh)
INSTANTIATE_RESOURCE_POOL(Model)
INSTANTIATE_RESOURCE_POOL(vk::Sampler)
INSTANTIATE_RESOURCE_POOL(Texture)
INSTANTIATE_RESOURCE_POOL(PipelineResource)
INSTANTIATE_RESOURCE_POOL(Material)
INSTANTIATE_RESOURCE_POOL(Audio)
INSTANTIATE_RESOURCE_POOL(Font)
INSTANTIATE_RESOURCE_POOL(Animation2D)
INSTANTIATE_RESOURCE_POOL(AnimatedMesh)
INSTANTIATE_RESOURCE_POOL(SkeletalAnimation)

} // namespace clay
