// class
#include "clay/application/common/Resources.h"

#define INSTANTIATE_RESOURCE_POOL(Type)                                         \
template struct Resources::ResourcePool<Type>;                                  \
                                                                                \
template Resources::Handle<Type>                                                \
    Resources::ResourcePool<Type>::loadResource(                                \
        const std::vector<std::string>&, const std::string&);                   \
                                                                                \
template Resources::Handle<Type>                                                \
    Resources::ResourcePool<Type>::add(const std::string&, Type&&);             \
                                                                                \
template void                                                                   \
    Resources::ResourcePool<Type>::remove(Resources::Handle<Type>);             \
                                                                                \
template Type&                                                                  \
    Resources::ResourcePool<Type>::operator[](Resources::Handle<Type>);         \
                                                                                \
template Resources::Handle<Type>                                                \
    Resources::ResourcePool<Type>::getHandle(const std::string&) const;


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

Resources::Resources(BaseGraphicsContext& graphicsContext) :
    mGraphicsContext_(graphicsContext) {};

Resources::~Resources() {
    releaseAll();
}

// TODO need logic for parsing meshes and materials
template<typename T>
void Resources::loadResource(const std::vector<std::string>& resourcePath, const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        utils::FileData loadedFile = loadFileToMemory(resourcePath[0]);

        std::vector<clay::Mesh> loadedMeshes;
        Mesh::parseObjFile(mGraphicsContext_, loadedFile, loadedMeshes);

        // TODO confirm there is only 1 mesh here // TODO mesh clean up seems wrong. There is delete happening due to the std::vector<clay::Mesh> being passed?
        std::unique_ptr<Mesh> meshPtr = std::make_unique<Mesh>(std::move(loadedMeshes[0]));
        mMeshes_[resourceName] = std::move(meshPtr);

    } else if constexpr(std::is_same_v<T, VkSampler>) {
    } else if constexpr(std::is_same_v<T, Model>) {
    } else if constexpr(std::is_same_v<T, Texture>) {
    } else if constexpr(std::is_same_v<T, PipelineResource>) {
    } else if constexpr(std::is_same_v<T, Material>) {
    } else if constexpr (std::is_same_v<T, Audio>) {
        utils::FileData loadedFile = loadFileToMemory(resourcePath[0]);
        mAudios_[resourceName] = std::make_unique<Audio>(loadedFile);
    }
    else if constexpr (std::is_same_v<T, Font>) {

    }
}

template<typename T>
void Resources::addResource(std::unique_ptr<T> resource, const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        mMeshes_[resourceName] = std::move(resource);
    } else if constexpr (std::is_same_v<T, Model>) {
        mModels_[resourceName] = std::move(resource);
    } else if constexpr (std::is_same_v<T, VkSampler>) {
        mSamplers_[resourceName] = std::move(resource);
    } else if constexpr (std::is_same_v<T, Texture>) {
        mTextures_[resourceName] = std::move(resource);
    } else if constexpr (std::is_same_v<T, PipelineResource>) {
        mPipelineResources_[resourceName] = std::move(resource);
    } else if constexpr (std::is_same_v<T, Material>) {
        mMaterials_[resourceName] = std::move(resource);
    } else if constexpr (std::is_same_v<T, Audio>) {
        mAudios_[resourceName] = std::move(resource);
    } else if constexpr (std::is_same_v<T, Font>) {
        mFonts_[resourceName] = std::move(resource);
    }
}

template<typename T>
T* Resources::getResource(const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        auto it = mMeshes_.find(resourceName);
        if (it != mMeshes_.end()) {
            return it->second.get();
        }
    } else if constexpr (std::is_same_v<T, Model>) {
        auto it = mModels_.find(resourceName);
        if (it != mModels_.end()) {
            return it->second.get();
        }
    } else if constexpr (std::is_same_v<T, VkSampler>) {
        auto it = mSamplers_.find(resourceName);
        if (it != mSamplers_.end()) {
            return it->second.get();
        }
    } else if constexpr (std::is_same_v<T, Texture>) {
        auto it = mTextures_.find(resourceName);
        if (it != mTextures_.end()) {
            return it->second.get();
        }
    } else if constexpr(std::is_same_v<T, PipelineResource>) {
        auto it = mPipelineResources_.find(resourceName);
        if (it != mPipelineResources_.end()) {
            return it->second.get();
        }
    } else if constexpr(std::is_same_v<T, Material>) {
        auto it = mMaterials_.find(resourceName);
        if (it != mMaterials_.end()) {
            return it->second.get();
        }
    } else if constexpr (std::is_same_v<T, Audio>) {
        auto it = mAudios_.find(resourceName);
        if (it != mAudios_.end()) {
            return it->second.get();
        }
    } else if constexpr (std::is_same_v<T, Font>) {
        auto it = mFonts_.find(resourceName);
        if (it != mFonts_.end()) {
            return it->second.get();
        }
    }

    // Return nullptr if resource not found or type mismatch
    return nullptr;
}

template<typename T>
void Resources::release(const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        auto it = mMeshes_.find(resourceName);
        if (it != mMeshes_.end()) {
            mMeshes_.erase(it);
        }
    } else if constexpr (std::is_same_v<T, Model>) {
        auto it = mModels_.find(resourceName);
        if (it != mModels_.end()) {
            mModels_.erase(it);
        }
    } else if constexpr(std::is_same_v<T, VkSampler>) {
        auto it = mSamplers_.find(resourceName);
        if (it != mSamplers_.end()) {
            mSamplers_.erase(it);
        }
    } else if constexpr(std::is_same_v<T, Texture>) {
        auto it = mTextures_.find(resourceName);
        if (it != mTextures_.end()) {
            mTextures_.erase(it);
        }
    } else if constexpr(std::is_same_v<T, PipelineResource>) {
        auto it = mPipelineResources_.find(resourceName);
        if (it != mPipelineResources_.end()) {
            mPipelineResources_.erase(it);
        }
    } else if constexpr(std::is_same_v<T, Material>) {
        auto it = mMaterials_.find(resourceName);
        if (it != mMaterials_.end()) {
            mMaterials_.erase(it);
        }
    } else if constexpr (std::is_same_v<T, Audio>) {
        auto it = mAudios_.find(resourceName);
        if (it != mAudios_.end()) {
            mAudios_.erase(it);
        }
    }
    else if constexpr (std::is_same_v<T, Font>) {
        auto it = mFonts_.find(resourceName);
        if (it != mFonts_.end()) {
            mFonts_.erase(it);
        }
    }
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

template<typename T>
Resources::Handle<T> Resources::ResourcePool<T>::loadResource(const std::vector<std::string>& resourcePaths, const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        utils::FileData loadedFile = loadFileToMemory(resourcePath[0]);

        std::vector<clay::Mesh> loadedMeshes;
        Mesh::parseObjFile(mGraphicsContext_, loadedFile, loadedMeshes);

        // TODO confirm there is only 1 mesh here // TODO mesh clean up seems wrong. There is delete happening due to the std::vector<clay::Mesh> being passed?
        // std::unique_ptr<Mesh> meshPtr = std::make_unique<Mesh>(std::move(loadedMeshes[0]));
        // mMeshes_[resourceName] = std::move(meshPtr);
        throw std::runtime_error("Load not implemented for Mesh");
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
        utils::FileData loadedFile = loadFileToMemory(resourcePath[0]);
        mAudios_[resourceName] = std::make_unique<Audio>(loadedFile);
    }
    else if constexpr (std::is_same_v<T, Font>) {
        throw std::runtime_error("Load not implemented for Font");
    } else {
        throw std::runtime_error("Load not implemented.");
    }
}

template<typename T>
Resources::Handle<T> Resources::ResourcePool<T>::add(const std::string& name, T&& obj) {
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
Resources::Handle<T> Resources::ResourcePool<T>::getHandle(const std::string& name) const
{
    auto it = name2Handle.find(name);
    if (it == name2Handle.end()) {
        throw std::runtime_error("Unknown resource: " + name);
    }
    return it->second;
}

// Explicit instantiate template for expected types
INSTANTIATE_RESOURCE_POOL(Mesh)
INSTANTIATE_RESOURCE_POOL(Model)
INSTANTIATE_RESOURCE_POOL(VkSampler)
INSTANTIATE_RESOURCE_POOL(Texture)
INSTANTIATE_RESOURCE_POOL(PipelineResource)
INSTANTIATE_RESOURCE_POOL(Material)
INSTANTIATE_RESOURCE_POOL(Audio)
INSTANTIATE_RESOURCE_POOL(Font)


/////////////////////
// template void Resources::loadResource<Mesh>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
// template void Resources::loadResource<Model>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
// template void Resources::loadResource<VkSampler>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
// template void Resources::loadResource<Texture>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
// template void Resources::loadResource<PipelineResource>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
// template void Resources::loadResource<Material>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
// template void Resources::loadResource<Audio>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
// template void Resources::loadResource<Font>(const std::vector<std::string>& resourcePath, const std::string& resourceName);

template void Resources::addResource(std::unique_ptr<Mesh> resource, const std::string& resourceName);
template void Resources::addResource(std::unique_ptr<Model> resource, const std::string& resourceName);
template void Resources::addResource(std::unique_ptr<VkSampler> resource, const std::string& resourceName);
template void Resources::addResource(std::unique_ptr<Texture> resource, const std::string& resourceName);
template void Resources::addResource(std::unique_ptr<Material> resource, const std::string& resourceName);
template void Resources::addResource(std::unique_ptr<PipelineResource> resource, const std::string& resourceName);
template void Resources::addResource(std::unique_ptr<Audio> resource, const std::string& resourceName);
template void Resources::addResource(std::unique_ptr<Font> resource, const std::string& resourceName);

template Mesh* Resources::getResource(const std::string& resourceName);
template Model* Resources::getResource(const std::string& resourceName);
template VkSampler* Resources::getResource(const std::string& resourceName);
template Texture* Resources::getResource(const std::string& resourceName);
template PipelineResource* Resources::getResource(const std::string& resourceName);
template Material* Resources::getResource(const std::string& resourceName);
template Audio* Resources::getResource(const std::string& resourceName);
template Font* Resources::getResource(const std::string& resourceName);

template void Resources::release<Mesh>(const std::string& resourceName);
template void Resources::release<Model>(const std::string& resourceName);
template void Resources::release<VkSampler>(const std::string& resourceName);
template void Resources::release<Texture>(const std::string& resourceName);
template void Resources::release<PipelineResource>(const std::string& resourceName);
template void Resources::release<Material>(const std::string& resourceName);
template void Resources::release<Audio>(const std::string& resourceName);
template void Resources::release<Font>(const std::string& resourceName);


} // namespace clay
