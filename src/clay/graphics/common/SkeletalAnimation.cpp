// standard lib
#include <algorithm>
// third party
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
// clay
#include <clay/graphics/common/SkeletalAnimation.h>

namespace clay {

SkeletalAnimation::SkeletalAnimation() {}

void SkeletalAnimation::setAnimationClip(const AnimationClip& clip) {
    mClip_ = clip;
    
    // Build bone channel map for fast lookup
    mBoneChannelMap_.clear();
    for (size_t i = 0; i < mClip_.channels.size(); ++i) {
        mBoneChannelMap_[mClip_.channels[i].boneName] = static_cast<int>(i);
    }
}

void SkeletalAnimation::setMesh(const AnimatedMesh* mesh) {
    mpMesh_ = mesh;
}

void SkeletalAnimation::play() {
    mIsPlaying_ = true;
}

void SkeletalAnimation::pause() {
    mIsPlaying_ = false;
}

void SkeletalAnimation::stop() {
    mIsPlaying_ = false;
    mCurrentTime_ = 0.0f;
}

void SkeletalAnimation::setLooping(bool loop) {
    mIsLooping_ = loop;
}

void SkeletalAnimation::setTime(float time) {
    mCurrentTime_ = time;
}

float SkeletalAnimation::getDuration() const {
    if (mClip_.ticksPerSecond > 0) {
        return static_cast<float>(mClip_.duration / mClip_.ticksPerSecond);
    }
    return static_cast<float>(mClip_.duration / 24.0); // Default 24 fps
}

std::vector<glm::mat4> SkeletalAnimation::update(float deltaTime) {
    if (!mpMesh_) {
        // Return identity matrices if no mesh
        return std::vector<glm::mat4>();
    }
    
    // Update time only if playing
    if (mIsPlaying_) {
        mCurrentTime_ += deltaTime;
        
        float duration = getDuration();
        if (mIsLooping_) {
            mCurrentTime_ = fmodf(mCurrentTime_, duration);
        } else {
            if (mCurrentTime_ >= duration) {
                mCurrentTime_ = duration;
                mIsPlaying_ = false;
            }
        }
    }
    
    // Calculate all bone transforms at current time (even when paused)
    const auto& bones = mpMesh_->getBones();
    std::vector<glm::mat4> boneTransforms(bones.size(), glm::mat4(1.0f));
    
    // Apply coordinate system conversion (Z-up to Y-up)
    glm::mat4 coordinateConversion = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    
    // Start from root bones (those with parentIndex == -1)
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].parentIndex == -1) {
            calculateBoneTransform(static_cast<int>(i), coordinateConversion, boneTransforms);
        }
    }
    
    return boneTransforms;
}

glm::vec3 SkeletalAnimation::interpolatePosition(const BoneChannel& channel, float animTime) {
    if (channel.positionKeys.size() == 1) {
        return channel.positionKeys[0].value;
    }
    
    int index = findPositionIndex(channel, animTime);
    int nextIndex = (index + 1) % channel.positionKeys.size();
    
    float deltaTime = static_cast<float>(channel.positionKeys[nextIndex].time - channel.positionKeys[index].time);
    float factor = (animTime - static_cast<float>(channel.positionKeys[index].time)) / deltaTime;
    factor = glm::clamp(factor, 0.0f, 1.0f);
    
    return glm::mix(channel.positionKeys[index].value, channel.positionKeys[nextIndex].value, factor);
}

glm::quat SkeletalAnimation::interpolateRotation(const BoneChannel& channel, float animTime) {
    if (channel.rotationKeys.size() == 1) {
        return channel.rotationKeys[0].value;
    }
    
    int index = findRotationIndex(channel, animTime);
    int nextIndex = (index + 1) % channel.rotationKeys.size();
    
    float deltaTime = static_cast<float>(channel.rotationKeys[nextIndex].time - channel.rotationKeys[index].time);
    float factor = (animTime - static_cast<float>(channel.rotationKeys[index].time)) / deltaTime;
    factor = glm::clamp(factor, 0.0f, 1.0f);
    
    return glm::slerp(channel.rotationKeys[index].value, channel.rotationKeys[nextIndex].value, factor);
}

glm::vec3 SkeletalAnimation::interpolateScale(const BoneChannel& channel, float animTime) {
    if (channel.scaleKeys.size() == 1) {
        return channel.scaleKeys[0].value;
    }
    
    int index = findScaleIndex(channel, animTime);
    int nextIndex = (index + 1) % channel.scaleKeys.size();
    
    float deltaTime = static_cast<float>(channel.scaleKeys[nextIndex].time - channel.scaleKeys[index].time);
    float factor = (animTime - static_cast<float>(channel.scaleKeys[index].time)) / deltaTime;
    factor = glm::clamp(factor, 0.0f, 1.0f);
    
    return glm::mix(channel.scaleKeys[index].value, channel.scaleKeys[nextIndex].value, factor);
}

int SkeletalAnimation::findPositionIndex(const BoneChannel& channel, float animTime) {
    for (size_t i = 0; i < channel.positionKeys.size() - 1; ++i) {
        if (animTime < static_cast<float>(channel.positionKeys[i + 1].time)) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(channel.positionKeys.size() - 1);
}

int SkeletalAnimation::findRotationIndex(const BoneChannel& channel, float animTime) {
    for (size_t i = 0; i < channel.rotationKeys.size() - 1; ++i) {
        if (animTime < static_cast<float>(channel.rotationKeys[i + 1].time)) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(channel.rotationKeys.size() - 1);
}

int SkeletalAnimation::findScaleIndex(const BoneChannel& channel, float animTime) {
    for (size_t i = 0; i < channel.scaleKeys.size() - 1; ++i) {
        if (animTime < static_cast<float>(channel.scaleKeys[i + 1].time)) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(channel.scaleKeys.size() - 1);
}

glm::mat4 SkeletalAnimation::getBoneTransform(const std::string& boneName, float animTime) {
    // Check if this bone has animation data
    auto it = mBoneChannelMap_.find(boneName);
    if (it == mBoneChannelMap_.end()) {
        // No animation for this bone, return identity
        return glm::mat4(1.0f);
    }
    
    const BoneChannel& channel = mClip_.channels[it->second];
    
    // Convert time to animation ticks
    float ticksPerSecond = mClip_.ticksPerSecond != 0 ? static_cast<float>(mClip_.ticksPerSecond) : 24.0f;
    float animTicks = animTime * ticksPerSecond;
    
    // Interpolate transformations
    glm::vec3 position = interpolatePosition(channel, animTicks);
    glm::quat rotation = interpolateRotation(channel, animTicks);
    glm::vec3 scale = interpolateScale(channel, animTicks);
    
    // Build transformation matrix
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 rotationMat = glm::toMat4(rotation);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
    
    return translation * rotationMat * scaleMat;
}

void SkeletalAnimation::calculateBoneTransform(int boneIndex, const glm::mat4& parentTransform, 
                                               std::vector<glm::mat4>& boneTransforms) {
    if (!mpMesh_) return;
    
    const auto& bones = mpMesh_->getBones();
    if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size())) return;
    
    const AnimatedMesh::BoneInfo& bone = bones[boneIndex];
    
    // Get animated transform for this bone
    glm::mat4 boneTransform = getBoneTransform(bone.name, mCurrentTime_);
    
    // Combine with parent transform
    glm::mat4 globalTransform = parentTransform * boneTransform;
    
    // Final transform = global * offset
    boneTransforms[boneIndex] = globalTransform * bone.offsetMatrix;
    
    // Process children
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].parentIndex == boneIndex) {
            calculateBoneTransform(static_cast<int>(i), globalTransform, boneTransforms);
        }
    }
}

} // namespace clay
