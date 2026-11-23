#pragma once
// standard lib
#include <vector>
#include <string>
#include <map>
// third party
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
// clay
#include <clay/graphics/common/AnimatedMesh.h>

namespace clay {

class SkeletalAnimation {
public:
    // Animation keyframe structures
    struct PositionKey {
        double time;
        glm::vec3 value;
    };

    struct RotationKey {
        double time;
        glm::quat value;
    };

    struct ScaleKey {
        double time;
        glm::vec3 value;
    };

    // Animation channel for a single bone
    struct BoneChannel {
        std::string boneName;
        std::vector<PositionKey> positionKeys;
        std::vector<RotationKey> rotationKeys;
        std::vector<ScaleKey> scaleKeys;
    };

    // Complete animation data
    struct AnimationClip {
        std::string name;
        double duration;           // in ticks
        double ticksPerSecond;     // animation speed
        std::vector<BoneChannel> channels;
    };

    SkeletalAnimation();

    // Load every animation clip contained in an FBX (an FBX can hold several named
    // takes, e.g. walk / idle_loop / kneel_action / Death01). Each clip's name is
    // the FBX animation-stack name. The mesh is loaded separately via
    // AnimatedMesh::loadFromFBX; set it on each clip's SkeletalAnimation.
    static std::vector<AnimationClip> loadClipsFromFBX(const std::string& filePath);

    void setAnimationClip(const AnimationClip& clip);
    void setMesh(const AnimatedMesh* mesh);
    
    // Update animation and return bone transforms
    std::vector<glm::mat4> update(float deltaTime);

    // Evaluate the pose at `timeSeconds` into `out` (resized to the mesh's bone count).
    // Does not touch the playback state, so one clip can be shared by many entities each
    // tracking its own time. Prefer this over setTime()+update(): it reuses the caller's
    // buffer (no per-frame allocation) and uses the precomputed bone/channel binding.
    void evaluate(float timeSeconds, std::vector<glm::mat4>& out) const;
    
    void play();
    void pause();
    void stop();
    void setLooping(bool loop);
    void setTime(float time);
    
    float getCurrentTime() const { return mCurrentTime_; }
    float getDuration() const;
    bool isPlaying() const { return mIsPlaying_; }

public:
    // Interpolation functions
    glm::vec3 interpolatePosition(const BoneChannel& channel, float animTime);
    glm::quat interpolateRotation(const BoneChannel& channel, float animTime);
    glm::vec3 interpolateScale(const BoneChannel& channel, float animTime);
    
    // Find keyframe indices
    int findPositionIndex(const BoneChannel& channel, float animTime);
    int findRotationIndex(const BoneChannel& channel, float animTime);
    int findScaleIndex(const BoneChannel& channel, float animTime);
    
    // Build transformation matrix for a bone
    glm::mat4 getBoneTransform(const std::string& boneName, float animTime);
    
    // Recursively calculate bone transforms with hierarchy
    void calculateBoneTransform(int boneIndex, const glm::mat4& parentTransform, 
                                std::vector<glm::mat4>& boneTransforms);

    const AnimatedMesh* mpMesh_ = nullptr;
    AnimationClip mClip_;
    std::map<std::string, int> mBoneChannelMap_;  // boneName -> channel index

private:
    // Clip<->skeleton binding, rebuilt lazily when the mesh or clip changes: the
    // channel driving each bone, and the bones in parent-before-child order, so a
    // pose is one linear pass with no name lookups or child searches.
    struct BoneBinding {
        int bone = -1;     // index into the mesh's bones
        int parent = -1;   // parent bone index (-1 = root)
        int channel = -1;  // index into mClip_.channels (-1 = not animated)
    };
    void rebuildBinding() const;
    mutable std::vector<BoneBinding> mBinding_;
    mutable bool mBindingDirty_ = true;

public:
    
    float mCurrentTime_ = 0.0f;
    bool mIsPlaying_ = false;
    bool mIsLooping_ = true;
};

} // namespace clay
