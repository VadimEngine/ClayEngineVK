// standard lib
// third party
#include <AL/alext.h>
#include <sndfile.h>
// project
#include "clay/utils/common/Logger.h"
// class
#include "clay/audio/AudioManager.h"


namespace clay {

AudioManager::AudioManager() {
    try {
        mpSoundDevice_ = std::make_unique<SoundDevice>();
        mpSoundSource_ = std::make_unique<SoundSource>();
        mAudioInitialized_ = true;
    } catch (const std::exception& e) {
        LOG_E("AudioManager error: %s", e.what());
    }
}

AudioManager::~AudioManager() {
    if (mAudioInitialized_ && mpSoundSource_) {
        alSourceStop(mpSoundSource_->getId()); 
        alSourcei(mpSoundSource_->getId(), AL_BUFFER, 0);
    }
}

void AudioManager::playSound(unsigned int audioId) {
    if (mAudioInitialized_) {
        mpSoundSource_->play(audioId);
    }
}

void AudioManager::stopSound() {
    if (mAudioInitialized_) {
        mpSoundSource_->stop();
    }
}

void AudioManager::pauseSound() {
    if (mAudioInitialized_) {
        mpSoundSource_->pause();
    }
}

void AudioManager::resumeSound() {
    if (mAudioInitialized_) {
        mpSoundSource_->resume();
    }
}

bool AudioManager::isPlaying() const {
    if (mAudioInitialized_) {
        return mpSoundSource_->isPlaying();
    }
    return false;
}

void AudioManager::setGain(float newGain) {
    if (mAudioInitialized_) {
        mpSoundSource_->setGain(newGain);
    }
}

float AudioManager::getGain() const {
    if (mAudioInitialized_) {
        return mpSoundSource_->getGain();
    } else {
        return 0;
    }
}

void AudioManager::clearSource() {
    if (mAudioInitialized_ && mpSoundSource_) {
        alSourceStop(mpSoundSource_->getId());
        alSourcei(mpSoundSource_->getId(), AL_BUFFER, 0);
    }
}

bool AudioManager::isInitialized() const {
    return mAudioInitialized_;
}


} // namespace clay