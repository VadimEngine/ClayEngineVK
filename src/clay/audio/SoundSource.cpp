// class
#include "clay/audio/SoundSource.h"

namespace clay {

SoundSource::SoundSource() {
    alGenSources(1, &mSource_);
    alSourcef(mSource_, AL_PITCH, mPitch_);
    alSourcef(mSource_, AL_GAIN, mGain_);
    alSource3f(mSource_, AL_POSITION, mPosition_[0], mPosition_[1], mPosition_[2]);
    alSource3f(mSource_, AL_VELOCITY, mVelocity[0], mVelocity[1], mVelocity[2]);
    alSourcei(mSource_, AL_LOOPING, mLoopSound_);
    alSourcei(mSource_, AL_BUFFER, mBuffer_);
}

SoundSource::~SoundSource() {
    alDeleteSources(1, &mSource_);
}

void SoundSource::play(const ALuint buffer_to_play) {
    // add the audio buffer to the source
    if (buffer_to_play != mBuffer_) {
        mBuffer_ = buffer_to_play;
        alSourcei(mSource_, AL_BUFFER, (ALint)mBuffer_);
    }

    alSourcePlay(mSource_);
}

void SoundSource::setGain(float newGain) {
    mGain_ = newGain;
    alSourcef(mSource_, AL_GAIN, mGain_);
}

float SoundSource::getGain() const {
    return mGain_;
}

ALuint SoundSource::getId() const {
    return mSource_;
}

} // namespace clay