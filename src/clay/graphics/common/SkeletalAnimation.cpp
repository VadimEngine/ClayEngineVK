// standard lib
#include <algorithm>
// third party
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// clay
#include <clay/utils/common/Logger.h>
#include <clay/graphics/common/SkeletalAnimation.h>

namespace clay {

SkeletalAnimation::SkeletalAnimation() {}

std::vector<SkeletalAnimation::AnimationClip>
SkeletalAnimation::loadClipsFromFBX(const std::string& filePath) {
    std::vector<AnimationClip> clips;
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals |
        aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        LOG_E("Assimp Error (clips): %s", importer.GetErrorString());
        return clips;
    }
    for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* animation = scene->mAnimations[a];
        AnimationClip clip;
        clip.name = animation->mName.C_Str();
        clip.duration = animation->mDuration;
        clip.ticksPerSecond = animation->mTicksPerSecond;
        for (unsigned int c = 0; c < animation->mNumChannels; ++c) {
            const aiNodeAnim* nodeAnim = animation->mChannels[c];
            BoneChannel channel;
            channel.boneName = nodeAnim->mNodeName.C_Str();
            for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys; ++i) {
                channel.positionKeys.push_back({ nodeAnim->mPositionKeys[i].mTime,
                    glm::vec3(nodeAnim->mPositionKeys[i].mValue.x,
                              nodeAnim->mPositionKeys[i].mValue.y,
                              nodeAnim->mPositionKeys[i].mValue.z) });
            }
            for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys; ++i) {
                channel.rotationKeys.push_back({ nodeAnim->mRotationKeys[i].mTime,
                    glm::quat(nodeAnim->mRotationKeys[i].mValue.w,
                              nodeAnim->mRotationKeys[i].mValue.x,
                              nodeAnim->mRotationKeys[i].mValue.y,
                              nodeAnim->mRotationKeys[i].mValue.z) });
            }
            for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys; ++i) {
                channel.scaleKeys.push_back({ nodeAnim->mScalingKeys[i].mTime,
                    glm::vec3(nodeAnim->mScalingKeys[i].mValue.x,
                              nodeAnim->mScalingKeys[i].mValue.y,
                              nodeAnim->mScalingKeys[i].mValue.z) });
            }
            clip.channels.push_back(std::move(channel));
        }
        clips.push_back(std::move(clip));
    }
    return clips;
}

void SkeletalAnimation::setAnimationClip(const AnimationClip& clip) {
    mClip_ = clip;

    // Build bone channel map for fast lookup
    mBoneChannelMap_.clear();
    for (size_t i = 0; i < mClip_.channels.size(); ++i) {
        mBoneChannelMap_[mClip_.channels[i].boneName] = static_cast<int>(i);
    }
    mBindingDirty_ = true;
}

void SkeletalAnimation::setMesh(const AnimatedMesh* mesh) {
    mpMesh_ = mesh;
    mBindingDirty_ = true;
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
    std::vector<glm::mat4> boneTransforms;
    evaluate(mCurrentTime_, boneTransforms);
    return boneTransforms;
}

namespace {

// Index of the keyframe at or before `ticks` (keys are sorted by time). Matches the
// original linear scan: the first i with ticks < keys[i+1].time, else the last key.
template<typename Key>
size_t keyIndex(const std::vector<Key>& keys, float ticks) {
    size_t lo = 0, hi = keys.size() - 1; // answer lies in [lo, hi]
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (ticks < static_cast<float>(keys[mid + 1].time)) hi = mid;
        else lo = mid + 1;
    }
    return lo;
}

// Blend factor between keys[i] and keys[i+1] at `ticks`, clamped to [0, 1].
template<typename Key>
float keyFactor(const std::vector<Key>& keys, size_t i, float ticks) {
    const float span = static_cast<float>(keys[i + 1].time - keys[i].time);
    if (span <= 0.0f) return 0.0f;
    return glm::clamp((ticks - static_cast<float>(keys[i].time)) / span, 0.0f, 1.0f);
}

// Position / scale keys (linear blend). At or after the last key the value holds.
template<typename Key>
glm::vec3 sampleVec3(const std::vector<Key>& keys, float ticks) {
    if (keys.size() == 1) return keys[0].value;
    const size_t i = keyIndex(keys, ticks);
    if (i + 1 >= keys.size()) return keys.back().value;
    return glm::mix(keys[i].value, keys[i + 1].value, keyFactor(keys, i, ticks));
}

glm::quat sampleQuat(const std::vector<SkeletalAnimation::RotationKey>& keys, float ticks) {
    if (keys.size() == 1) return keys[0].value;
    const size_t i = keyIndex(keys, ticks);
    if (i + 1 >= keys.size()) return keys.back().value;
    return glm::slerp(keys[i].value, keys[i + 1].value, keyFactor(keys, i, ticks));
}

} // namespace

void SkeletalAnimation::rebuildBinding() const {
    mBinding_.clear();
    mBindingDirty_ = false;
    if (!mpMesh_) return;

    const auto& bones = mpMesh_->getBones();
    mBinding_.reserve(bones.size());

    // Breadth-first from the roots gives parent-before-child order. Bones not
    // reachable from a root (a broken parent chain) are left out and stay identity,
    // exactly as the recursive walk treated them.
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].parentIndex == -1) mBinding_.push_back({ static_cast<int>(i), -1, -1 });
    }
    for (size_t head = 0; head < mBinding_.size(); ++head) {
        const int parent = mBinding_[head].bone;
        for (size_t i = 0; i < bones.size(); ++i) {
            if (bones[i].parentIndex == parent) mBinding_.push_back({ static_cast<int>(i), parent, -1 });
        }
    }
    for (BoneBinding& b : mBinding_) {
        const auto it = mBoneChannelMap_.find(bones[b.bone].name);
        if (it != mBoneChannelMap_.end()) b.channel = it->second;
    }
}

void SkeletalAnimation::evaluate(float timeSeconds, std::vector<glm::mat4>& out) const {
    if (!mpMesh_) { out.clear(); return; }
    if (mBindingDirty_) rebuildBinding();

    const auto& bones = mpMesh_->getBones();
    out.assign(bones.size(), glm::mat4(1.0f));

    // Global (model-space) transform of each bone; parents are filled before children.
    static thread_local std::vector<glm::mat4> globals;
    globals.resize(bones.size());

    // Coordinate system conversion (Z-up to Y-up), applied at the roots.
    static const glm::mat4 kCoordinateConversion =
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    const float ticksPerSecond = mClip_.ticksPerSecond != 0 ? static_cast<float>(mClip_.ticksPerSecond) : 24.0f;
    const float ticks = timeSeconds * ticksPerSecond;

    // ... followed by whatever the mesh says sits above its root bones (identity for most).
    const glm::mat4 rootTransform = kCoordinateConversion * mpMesh_->getRootTransform();

    for (const BoneBinding& b : mBinding_) {
        const glm::mat4& parent = (b.parent < 0) ? rootTransform : globals[b.parent];
        if (b.channel < 0) {
            globals[b.bone] = parent * bones[b.bone].localBind; // not animated: stays in its rest pose
        } else {
            const BoneChannel& channel = mClip_.channels[b.channel];
            // local = translate * rotate * scale, composed directly.
            glm::mat4 local = glm::mat4_cast(sampleQuat(channel.rotationKeys, ticks));
            const glm::vec3 scale = sampleVec3(channel.scaleKeys, ticks);
            local[0] *= scale.x;
            local[1] *= scale.y;
            local[2] *= scale.z;
            local[3] = glm::vec4(sampleVec3(channel.positionKeys, ticks), 1.0f);
            globals[b.bone] = parent * local;
        }
        out[b.bone] = globals[b.bone] * bones[b.bone].offsetMatrix;
    }
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
