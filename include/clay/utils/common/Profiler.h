#pragma once
// standard lib
#include <chrono>
#include <cstdint>
#include <vector>

namespace clay {

// Lightweight frame profiler. Named scopes accumulate CPU milliseconds and named
// counters accumulate values over a frame; every `kWindowFrames` frames the per-frame
// averages are latched into a snapshot that a debug HUD can read (and that is logged
// when logging is on). Costs one branch per scope while disabled.
//
//   CLAY_PROFILE_SCOPE("Shadow pass");        // times the enclosing block
//   CLAY_PROFILE_COUNT("Draw calls", n);      // adds n to this frame's counter
//
// Names MUST be string literals (they are stored and compared by pointer first).
// Enable with Profiler::get().setEnabled(true), or set the CLAY_PROFILE=1 environment
// variable (which also logs each snapshot to stdout). Main-thread only.
class Profiler {
public:
    struct Entry {
        const char* name = nullptr;
        bool isCounter = false;
        double avg = 0.0;   // ms per frame (scope) or value per frame (counter)
        double peak = 0.0;  // worst single frame in the window
    };

    static constexpr int kWindowFrames = 120;

    static Profiler& get();

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled_; }
    void setLogging(bool logging) { mLogging_ = logging; }

    void addTime(const char* name, double ms) { add(name, ms, false); }
    void addCount(const char* name, double value) { add(name, value, true); }

    // Call once per frame (the app does this). Latches a snapshot every window.
    void endFrame();

    // Averages over the last completed window, in first-seen order.
    const std::vector<Entry>& snapshot() const { return mSnapshot_; }
    // Whole-frame time (ms) averaged over the last completed window.
    double frameMs() const { return mFrameMs_; }

private:
    struct Slot {
        const char* name = nullptr;
        bool isCounter = false;
        double frame = 0.0;  // accumulated this frame
        double total = 0.0;  // accumulated this window
        double peak = 0.0;   // largest single-frame value this window
    };

    Profiler();
    void add(const char* name, double value, bool isCounter);
    Slot& slot(const char* name, bool isCounter);

    bool mEnabled_ = false;
    bool mLogging_ = false;
    int mFrames_ = 0;
    std::vector<Slot> mSlots_;
    std::vector<Entry> mSnapshot_;
    double mFrameMs_ = 0.0;
    std::chrono::steady_clock::time_point mWindowStart_;
};

class ProfileScope {
public:
    explicit ProfileScope(const char* name)
        : mName_(Profiler::get().isEnabled() ? name : nullptr) {
        if (mName_) mStart_ = std::chrono::steady_clock::now();
    }
    ~ProfileScope() {
        if (!mName_) return;
        const std::chrono::duration<double, std::milli> ms = std::chrono::steady_clock::now() - mStart_;
        Profiler::get().addTime(mName_, ms.count());
    }
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

private:
    const char* mName_;
    std::chrono::steady_clock::time_point mStart_;
};

} // namespace clay

#define CLAY_PROFILE_CONCAT_INNER(a, b) a##b
#define CLAY_PROFILE_CONCAT(a, b) CLAY_PROFILE_CONCAT_INNER(a, b)
#define CLAY_PROFILE_SCOPE(name) ::clay::ProfileScope CLAY_PROFILE_CONCAT(clayProfileScope_, __LINE__)(name)
#define CLAY_PROFILE_COUNT(name, value)                                              \
    do {                                                                             \
        if (::clay::Profiler::get().isEnabled())                                     \
            ::clay::Profiler::get().addCount(name, static_cast<double>(value));      \
    } while (0)
