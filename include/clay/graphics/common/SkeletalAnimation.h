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
    
    void setAnimationClip(const AnimationClip& clip);
    void setMesh(const AnimatedMesh* mesh);
    
    // Update animation and return bone transforms
    std::vector<glm::mat4> update(float deltaTime);
    
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
    
    float mCurrentTime_ = 0.0f;
    bool mIsPlaying_ = false;
    bool mIsLooping_ = true;
};

} // namespace clay
