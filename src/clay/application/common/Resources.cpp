// class
#include "clay/application/common/Resources.h"

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

Resources::Resources(IGraphicsContext& graphicsContext) :
    mGraphicsContext_(graphicsContext) {};

Resources::~Resources() = default;

// TODO need logic for parsing meshes and materials
template<typename T>
void Resources::loadResource(const std::vector<std::string>& resourcePath, const std::string& resourceName) {
    if constexpr (std::is_same_v<T, Mesh>) {
        utils::FileData loadedFile = loadFileToMemory(resourcePath[0]);

        std::vector<clay::Mesh> loadedMeshes;
        Mesh::parseObjFile(mGraphicsContext_, loadedFile, loadedMeshes);

        // TODO confirm there is only 1 mesh here
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
    mMeshes_.clear();
    mModels_.clear();
    mSamplers_.clear();
    mTextures_.clear();
    mPipelineResources_.clear();
    mMaterials_.clear();
    mAudios_.clear();
    mFonts_.clear();
}

// Explicit instantiate template for expected types
template void Resources::loadResource<Mesh>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
template void Resources::loadResource<Model>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
template void Resources::loadResource<VkSampler>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
template void Resources::loadResource<Texture>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
template void Resources::loadResource<PipelineResource>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
template void Resources::loadResource<Material>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
template void Resources::loadResource<Audio>(const std::vector<std::string>& resourcePath, const std::string& resourceName);
template void Resources::loadResource<Font>(const std::vector<std::string>& resourcePath, const std::string& resourceName);

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
