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
    alSourceStop(mSource_);
    alSourcei(mSource_, AL_BUFFER, 0);
    alDeleteSources(1, &mSource_);
}

void SoundSource::play(const ALuint buffer_to_play) {
    // A source must be in the INITIAL or STOPPED state to (re)attach a buffer. Setting
    // AL_BUFFER on a source that is still AL_PLAYING/AL_PAUSED raises AL_INVALID_OPERATION
    // and is SILENTLY IGNORED — the old buffer stays attached, so alSourcePlay then
    // restarts the PREVIOUS sound instead of the requested one. On a reused/pooled source
    // this makes one-shots "repeat for a while" and footsteps come out as the wrong sample.
    // Always stop first so the new buffer is guaranteed to attach. (Stop on an already-
    // stopped source is a harmless no-op.) We also drop the "only if different" guard: even
    // when the same buffer is requested, re-attaching after the stop keeps mBuffer_ and the
    // actual AL buffer in sync and lets the sample restart cleanly from the beginning.
    alSourceStop(mSource_);
    mBuffer_ = buffer_to_play;
    alSourcei(mSource_, AL_BUFFER, (ALint)mBuffer_);
    alSourcePlay(mSource_);
}

void SoundSource::stop() {
    alSourceStop(mSource_);
}

void SoundSource::pause() {
    alSourcePause(mSource_);
}

void SoundSource::resume() {
    alSourcePlay(mSource_);
}

bool SoundSource::isPlaying() const {
    ALint state;
    alGetSourcei(mSource_, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

void SoundSource::setGain(float newGain) {
    mGain_ = newGain;
    alSourcef(mSource_, AL_GAIN, mGain_);
}

float SoundSource::getGain() const {
    return mGain_;
}

void SoundSource::setLooping(bool loop) {
    mLoopSound_ = loop;
    alSourcei(mSource_, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
}

ALuint SoundSource::getId() const {
    return mSource_;
}

} // namespace clay