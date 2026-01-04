#pragma once
/**
 * @file TestHelpers.h
 * @brief Common test utilities and mocks for ClayEngineVK tests
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace clay::test {

/**
 * @brief Base test fixture for ClayEngineVK tests
 * 
 * Provides common setup/teardown and utilities for all tests
 */
class ClayTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all tests
    }

    void TearDown() override {
        // Common cleanup for all tests
    }
};

/**
 * @brief Test fixture specifically for Desktop platform tests
 */
class DesktopTestBase : public ClayTestBase {
protected:
    void SetUp() override {
        ClayTestBase::SetUp();
        // Desktop-specific setup
    }

    void TearDown() override {
        // Desktop-specific cleanup
        ClayTestBase::TearDown();
    }
};

/**
 * @brief Test fixture specifically for Android platform tests
 */
class AndroidTestBase : public ClayTestBase {
protected:
    void SetUp() override {
        ClayTestBase::SetUp();
        // Android-specific setup
    }

    void TearDown() override {
        // Android-specific cleanup
        ClayTestBase::TearDown();
    }
};

/**
 * @brief Test fixture specifically for XR platform tests
 */
class XRTestBase : public ClayTestBase {
protected:
    void SetUp() override {
        ClayTestBase::SetUp();
        // XR-specific setup
    }

    void TearDown() override {
        // XR-specific cleanup
        ClayTestBase::TearDown();
    }
};

} // namespace clay::test
