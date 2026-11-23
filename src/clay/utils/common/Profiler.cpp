// standard lib
#include <cstdio>
#include <cstdlib>
#include <cstring>
// class
#include "clay/utils/common/Profiler.h"

namespace clay {

Profiler& Profiler::get() {
    static Profiler instance;
    return instance;
}

Profiler::Profiler() {
    // CLAY_PROFILE=1 turns profiling + stdout logging on from launch (for captures
    // without touching the game's UI).
    if (const char* env = std::getenv("CLAY_PROFILE"); env && env[0] != '\0' && env[0] != '0') {
        mLogging_ = true;
        setEnabled(true);
    }
}

void Profiler::setEnabled(bool enabled) {
    if (enabled == mEnabled_) return;
    mEnabled_ = enabled;
    mFrames_ = 0;
    for (Slot& s : mSlots_) { s.frame = 0.0; s.total = 0.0; s.peak = 0.0; }
    if (!enabled) mSnapshot_.clear();
    mWindowStart_ = std::chrono::steady_clock::now();
}

Profiler::Slot& Profiler::slot(const char* name, bool isCounter) {
    for (Slot& s : mSlots_) if (s.name == name) return s;                  // same literal
    for (Slot& s : mSlots_) if (std::strcmp(s.name, name) == 0) return s;  // same text
    Slot s;
    s.name = name;
    s.isCounter = isCounter;
    mSlots_.push_back(s);
    return mSlots_.back();
}

void Profiler::add(const char* name, double value, bool isCounter) {
    if (!mEnabled_) return;
    slot(name, isCounter).frame += value;
}

void Profiler::endFrame() {
    if (!mEnabled_) return;

    for (Slot& s : mSlots_) {
        s.total += s.frame;
        if (s.frame > s.peak) s.peak = s.frame;
        s.frame = 0.0;
    }
    if (++mFrames_ < kWindowFrames) return;

    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> windowMs = now - mWindowStart_;
    mFrameMs_ = windowMs.count() / mFrames_;

    mSnapshot_.clear();
    mSnapshot_.reserve(mSlots_.size());
    for (Slot& s : mSlots_) {
        mSnapshot_.push_back({ s.name, s.isCounter, s.total / mFrames_, s.peak });
        s.total = 0.0;
        s.peak = 0.0;
    }

    if (mLogging_) {
        std::printf("[profile] frame %.3f ms (%.1f fps)\n", mFrameMs_, mFrameMs_ > 0.0 ? 1000.0 / mFrameMs_ : 0.0);
        for (const Entry& e : mSnapshot_) {
            if (e.isCounter) std::printf("[profile]   %-28s %10.1f      (peak %.0f)\n", e.name, e.avg, e.peak);
            else             std::printf("[profile]   %-28s %10.3f ms   (peak %.3f)\n", e.name, e.avg, e.peak);
        }
        std::fflush(stdout);
    }

    mFrames_ = 0;
    mWindowStart_ = now;
}

} // namespace clay
