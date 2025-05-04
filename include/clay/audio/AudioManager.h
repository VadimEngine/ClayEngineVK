#pragma once
// standard lib
#include <inttypes.h>
#include <memory>
#include <stdexcept>
// third party
// project
#include "clay/audio/SoundDevice.h"
#include "clay/audio/SoundSource.h"

namespace clay {

class AudioManager {
public:
    /** Constructor */
    AudioManager();

    /** Destructor */
    ~AudioManager();

    /**
     * Play the audio if it has been loaded
     * @param audioId Audio buffer id
     */
    void playSound(unsigned int audioId);

    /**
     * Set the gain of the audio source
     * @param newGain Gain to set (0-1.0f)
     */
    void setGain(float newGain);

    /** Get the current Gain */
    float getGain() const;

    bool isInitialized() const;

private:
    /** Sound Device*/
    std::unique_ptr<SoundDevice> mpSoundDevice_;
    /** Sound Source */
    std::unique_ptr<SoundSource> mpSoundSource_;
    /** If audio was set up successfully */
    bool mAudioInitialized_ = false;
};

} // namespace clay