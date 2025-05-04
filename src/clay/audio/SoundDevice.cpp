// standard lib
// third party
// project
#include "clay/utils/common/Logger.h"
// class
#include "clay/audio/SoundDevice.h"

namespace clay {

SoundDevice::SoundDevice() {
    mpALCDevice_ = alcOpenDevice(nullptr); // nullptr = get default device
    if (!mpALCDevice_) {
        throw std::runtime_error("Failed to get sound device");
    }
    // create context
    mpALCContext_ = alcCreateContext(mpALCDevice_, nullptr);
    if (!mpALCContext_) {
        throw std::runtime_error("Failed to set sound context");
    }
    // make context current
    if (!alcMakeContextCurrent(mpALCContext_)) {
        throw std::runtime_error("Failed to make context current");
    }
    const ALCchar* name = nullptr;
    if (alcIsExtensionPresent(mpALCDevice_, "ALC_ENUMERATE_ALL_EXT")) {
        name = alcGetString(mpALCDevice_, ALC_ALL_DEVICES_SPECIFIER);
    }
    if (!name || alcGetError(mpALCDevice_) != AL_NO_ERROR) {
        name = alcGetString(mpALCDevice_, ALC_DEVICE_SPECIFIER);
    }
    LOG_I("Opened \"%s\"\n", name);
}

SoundDevice::~SoundDevice() {
    if (!alcMakeContextCurrent(nullptr)) {
        throw std::runtime_error("failed to set context to nullptr");
    }
    alcDestroyContext(mpALCContext_);
    ALenum err = alcGetError(mpALCDevice_);
    if (err != AL_NO_ERROR) {
       throw std::runtime_error("failed to unset audio context during close");
    }
    if (!alcCloseDevice(mpALCDevice_)) {
        throw std::runtime_error("failed to close sound device");
    }
}

} // namespace clay