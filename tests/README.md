# ClayEngineVK Tests

This directory contains the test suite for ClayEngineVK, organized by platform and test type.

### Platform-Specific Tests

Tests are organized by platform to ensure that platform-specific code works correctly on each target:

- **Desktop** (`unit/desktop/`) - Tests for Windows and Linux
  - Window management
  - AppDesktop functionality
  - Desktop input handling

- **Android** (`unit/android/`) - Tests for Android devices
  - AppAndroid functionality
  - Android-specific graphics context
  - Touch input

- **XR** (`unit/xr/`) - Tests for Oculus Quest and other XR platforms
  - AppXR functionality
  - XR-specific graphics context
  - XR input and tracking

### Cross-Platform Tests

Tests in the root `unit/` directory test functionality that should work the same across all platforms:

- **Resources** - Resource management, loading, and cleanup
- **ECS** - Entity Component System functionality
- **Math** - Vector, matrix, and quaternion operations (future)
- **Audio** - Audio system (future)

## Running Tests

### Quick Start (Desktop)

```bash
# From ClayEngineVK root directory
cmake -S . -B build -DCLAY_PLATFORM_DESKTOP=ON -DCLAYENGINE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Platform-Specific Tests

#### Windows/Linux (Desktop)
```bash
cmake -S . -B build -DCLAY_PLATFORM_DESKTOP=ON -DCLAYENGINE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -L desktop --output-on-failure
```

#### Android
```bash
cmake -S . -B build -DCLAY_PLATFORM_ANDROID=ON -DCLAYENGINE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -L android --output-on-failure
```

#### XR (Oculus Quest)
```bash
cmake -S . -B build -DCLAY_PLATFORM_XR=ON -DCLAYENGINE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -L xr --output-on-failure
```

### Running Specific Tests

```bash
# Run only common (cross-platform) tests
ctest --test-dir build -L common --output-on-failure

# Run specific test executable
./build/tests/ClayEngineVK_DesktopTests

# Run specific test case
./build/tests/ClayEngineVK_DesktopTests --gtest_filter=AppDesktopTest.CompilationTest
```

### Verbose Output

```bash
# Run tests with verbose output
ctest --test-dir build --verbose

# Run with GoogleTest verbose output
./build/tests/ClayEngineVK_DesktopTests --gtest_verbose
```

## Writing New Tests

### Test File Naming Convention

- Platform-specific: `<Feature>Test.cpp` in `unit/<platform>/`
- Cross-platform: `<Feature>Test.cpp` in `unit/`
- Example: `AppDesktopTest.cpp`, `ResourcesTest.cpp`, `PhysicsTest.cpp`

### Basic Test Structure

```cpp
#include <gtest/gtest.h>
#include "../TestHelpers.h"

// For platform-specific tests
#ifdef CLAY_PLATFORM_DESKTOP

namespace clay::test {

class MyFeatureTest : public DesktopTestBase {
protected:
    void SetUp() override {
        DesktopTestBase::SetUp();
        // Your setup code
    }
    
    void TearDown() override {
        // Your cleanup code
        DesktopTestBase::TearDown();
    }
};

TEST_F(MyFeatureTest, BasicFunctionality) {
    // Your test code
    EXPECT_TRUE(true);
}

} // namespace clay::test

#endif // CLAY_PLATFORM_DESKTOP
```
