/**
 * @file AppXRTest.cpp
 * @brief Tests for XR Application (clay::AppXR / Oculus Quest)
 * 
 * Test Case: Verify that XR application classes compile and
 * basic structure is correct.
 */

#include <gtest/gtest.h>
#include "TestHelpers.h"

#ifdef CLAY_PLATFORM_XR

#include <clay/application/xr/AppXR.h>

namespace clay::test {

/**
 * @brief Test fixture for AppXR tests
 */
class AppXRTest : public XRTestBase {
protected:
    void SetUp() override {
        XRTestBase::SetUp();
    }

    void TearDown() override {
        XRTestBase::TearDown();
    }
};

/**
 * @test AppXR_BasicCompilation
 * @brief Verify that AppXR compiles successfully
 * 
 * Placeholder test for XR platform.
 * TODO: Add functional tests for XR-specific behavior:
 * - OpenXR session management
 * - Head/controller tracking
 * - Stereo rendering
 * - XR input handling
 */
TEST_F(AppXRTest, BasicCompilation) {
    SUCCEED() << "AppXR compiled successfully";
}

} // namespace clay::test

#endif // CLAY_PLATFORM_XR
