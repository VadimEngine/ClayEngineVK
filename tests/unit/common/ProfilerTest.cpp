// clay::Profiler: scopes and counters add up over a frame, and every kWindowFrames frames
// the per-frame averages and the worst frame are latched into a snapshot (what a debug HUD
// shows). The profiler is one shared object and keeps every name it has seen, so each test
// here uses names of its own and looks its entries up by name.
#include <gtest/gtest.h>
// standard lib
#include <cstring>
// clay
#include "clay/utils/common/Profiler.h"

namespace {

using clay::Profiler;

class ProfilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Profiler::get().setLogging(false);
        Profiler::get().setEnabled(false);  // (a fresh window, whatever ran before)
        Profiler::get().setEnabled(true);
    }
    void TearDown() override { Profiler::get().setEnabled(false); }

    // Ends the frames left in the current window, so a snapshot is latched.
    static void finishWindow(int framesAlreadyEnded) {
        for (int i = framesAlreadyEnded; i < Profiler::kWindowFrames; ++i) Profiler::get().endFrame();
    }

    static const Profiler::Entry* find(const char* name) {
        for (const Profiler::Entry& e : Profiler::get().snapshot()) {
            if (std::strcmp(e.name, name) == 0) return &e;
        }
        return nullptr;
    }
};

} // namespace

TEST_F(ProfilerTest, NoSnapshotBeforeAWindowCompletes) {
    for (int i = 0; i < Profiler::kWindowFrames - 1; ++i) {
        Profiler::get().addCount("test.early", 1.0);
        Profiler::get().endFrame();
    }
    EXPECT_EQ(find("test.early"), nullptr);
    Profiler::get().endFrame();
    EXPECT_NE(find("test.early"), nullptr);
}

TEST_F(ProfilerTest, CounterAverageIsPerFrame) {
    for (int i = 0; i < Profiler::kWindowFrames; ++i) {
        // Several adds in one frame count together.
        Profiler::get().addCount("test.draws", 2.0);
        Profiler::get().addCount("test.draws", 3.0);
        Profiler::get().endFrame();
    }
    const Profiler::Entry* entry = find("test.draws");
    ASSERT_NE(entry, nullptr);
    EXPECT_TRUE(entry->isCounter);
    EXPECT_DOUBLE_EQ(entry->avg, 5.0);
    EXPECT_DOUBLE_EQ(entry->peak, 5.0);
}

// The peak is the worst single frame, which an average hides: one 60 ms hitch among
// 1 ms frames.
TEST_F(ProfilerTest, PeakIsTheWorstFrame) {
    for (int i = 0; i < Profiler::kWindowFrames; ++i) {
        Profiler::get().addTime("test.hitch", i == 17 ? 60.0 : 1.0);
        Profiler::get().endFrame();
    }
    const Profiler::Entry* entry = find("test.hitch");
    ASSERT_NE(entry, nullptr);
    EXPECT_FALSE(entry->isCounter);
    EXPECT_DOUBLE_EQ(entry->peak, 60.0);
    EXPECT_DOUBLE_EQ(entry->avg, (60.0 + (Profiler::kWindowFrames - 1) * 1.0) / Profiler::kWindowFrames);
}

// A frame in which something was not measured counts as zero, not as missing.
TEST_F(ProfilerTest, FramesWithoutTheEventStillCountInTheAverage) {
    Profiler::get().addCount("test.rare", 240.0);
    Profiler::get().endFrame();
    finishWindow(1);
    const Profiler::Entry* entry = find("test.rare");
    ASSERT_NE(entry, nullptr);
    EXPECT_DOUBLE_EQ(entry->avg, 240.0 / Profiler::kWindowFrames);
}

TEST_F(ProfilerTest, EachWindowStartsFromZero) {
    for (int i = 0; i < Profiler::kWindowFrames; ++i) {
        Profiler::get().addCount("test.windowed", 10.0);
        Profiler::get().endFrame();
    }
    ASSERT_NE(find("test.windowed"), nullptr);
    EXPECT_DOUBLE_EQ(find("test.windowed")->avg, 10.0);

    // A second, quieter window does not carry anything over.
    for (int i = 0; i < Profiler::kWindowFrames; ++i) {
        Profiler::get().addCount("test.windowed", 1.0);
        Profiler::get().endFrame();
    }
    EXPECT_DOUBLE_EQ(find("test.windowed")->avg, 1.0);
    EXPECT_DOUBLE_EQ(find("test.windowed")->peak, 1.0);
}

// Names are matched by their text, not only by the literal's address: the same name
// written in two places is one entry.
TEST_F(ProfilerTest, SameNameFromTwoPlacesIsOneEntry) {
    const char first[] = "test.shared";
    const char second[] = "test.shared";
    ASSERT_NE(static_cast<const void*>(first), static_cast<const void*>(second));
    Profiler::get().addCount(first, 1.0);
    Profiler::get().addCount(second, 1.0);
    Profiler::get().endFrame();
    finishWindow(1);

    int entries = 0;
    for (const Profiler::Entry& e : Profiler::get().snapshot()) entries += std::strcmp(e.name, "test.shared") == 0;
    EXPECT_EQ(entries, 1);
    EXPECT_DOUBLE_EQ(find("test.shared")->avg, 2.0 / Profiler::kWindowFrames);
}

TEST_F(ProfilerTest, ScopeMacroTimesItsBlock) {
    {
        CLAY_PROFILE_SCOPE("test.scope");
        volatile double sink = 0.0;
        for (int i = 0; i < 1000; ++i) sink = sink + i;
    }
    CLAY_PROFILE_COUNT("test.macroCount", 4);
    Profiler::get().endFrame();
    finishWindow(1);

    const Profiler::Entry* scope = find("test.scope");
    ASSERT_NE(scope, nullptr);
    EXPECT_FALSE(scope->isCounter);
    EXPECT_GE(scope->peak, 0.0);
    const Profiler::Entry* count = find("test.macroCount");
    ASSERT_NE(count, nullptr);
    EXPECT_TRUE(count->isCounter);
    EXPECT_DOUBLE_EQ(count->peak, 4.0);
}

// Off is the normal state in a shipped game: nothing is recorded and nothing is reported.
TEST_F(ProfilerTest, DisabledRecordsNothing) {
    for (int i = 0; i < Profiler::kWindowFrames; ++i) {
        Profiler::get().addCount("test.beforeOff", 1.0);
        Profiler::get().endFrame();
    }
    ASSERT_FALSE(Profiler::get().snapshot().empty());

    Profiler::get().setEnabled(false);
    EXPECT_TRUE(Profiler::get().snapshot().empty()) << "turning it off clears what the HUD shows";
    for (int i = 0; i < Profiler::kWindowFrames; ++i) {
        Profiler::get().addCount("test.whileOff", 1.0);
        { CLAY_PROFILE_SCOPE("test.scopeWhileOff"); }
        Profiler::get().endFrame();
    }
    EXPECT_TRUE(Profiler::get().snapshot().empty());

    // Back on: what was added while it was off never shows up.
    Profiler::get().setEnabled(true);
    finishWindow(0);
    EXPECT_EQ(find("test.whileOff"), nullptr);
    EXPECT_EQ(find("test.scopeWhileOff"), nullptr);
    ASSERT_NE(find("test.beforeOff"), nullptr);
    EXPECT_DOUBLE_EQ(find("test.beforeOff")->avg, 0.0);
}
