# ClayEngineVK Tests
cmake_minimum_required(VERSION 3.12)

message(STATUS "Building ClayEngineVK Tests")

# Use existing googletest submodule
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/googletest ${CMAKE_CURRENT_BINARY_DIR}/googletest EXCLUDE_FROM_ALL)

# Include ClayEngineVK headers
include_directories(${CMAKE_SOURCE_DIR}/include)

# Include tests directory for TestHelpers.h
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Common test utilities
set(TEST_COMMON_SOURCES
    TestHelpers.h
)

# ==============================================================================
# Platform-Specific Tests
# ==============================================================================

# Desktop Tests (Windows, Linux)
if(CLAY_PLATFORM_DESKTOP)
    message(STATUS "Adding Desktop platform tests")
    
    set(DESKTOP_TEST_SOURCES
        unit/desktop/AppDesktopTest.cpp
    )
    
    add_executable(ClayEngineVK_DesktopTests
        ${DESKTOP_TEST_SOURCES}
        ${TEST_COMMON_SOURCES}
    )
    
    target_link_libraries(ClayEngineVK_DesktopTests
        PRIVATE
        ClayEngineVK
        gtest_main
        gmock_main
    )
    
    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(ClayEngineVK_DesktopTests
        PROPERTIES
        LABELS "desktop"
    )
endif()

# Android Tests
if(CLAY_PLATFORM_ANDROID)
    message(STATUS "Adding Android platform tests")
    
    set(ANDROID_TEST_SOURCES
        unit/android/AppAndroidTest.cpp
    )
    
    add_executable(ClayEngineVK_AndroidTests
        ${ANDROID_TEST_SOURCES}
        ${TEST_COMMON_SOURCES}
    )
    
    target_link_libraries(ClayEngineVK_AndroidTests
        PRIVATE
        ClayEngineVK
        gtest_main
        gmock_main
    )
    
    include(GoogleTest)
    gtest_discover_tests(ClayEngineVK_AndroidTests
        PROPERTIES
        LABELS "android"
    )
endif()

# XR Tests (Oculus Quest, etc.)
if(CLAY_PLATFORM_XR)
    message(STATUS "Adding XR platform tests")
    
    set(XR_TEST_SOURCES
        unit/xr/AppXRTest.cpp
    )
    
    add_executable(ClayEngineVK_XRTests
        ${XR_TEST_SOURCES}
        ${TEST_COMMON_SOURCES}
    )
    
    target_link_libraries(ClayEngineVK_XRTests
        PRIVATE
        ClayEngineVK
        gtest_main
        gmock_main
    )
    
    include(GoogleTest)
    gtest_discover_tests(ClayEngineVK_XRTests
        PROPERTIES
        LABELS "xr"
    )
endif()

# ==============================================================================
# Common/Cross-Platform Tests
# ==============================================================================

# These tests work the same on every platform and need no window or graphics device:
# maths, conventions and bookkeeping (unit/common/).
set(COMMON_TEST_SOURCES
    unit/common/CameraTest.cpp
    unit/common/CollisionTest.cpp
    unit/common/ECSTest.cpp
    unit/common/FrustumTest.cpp
    unit/common/ProfilerTest.cpp
    unit/common/ResourcesTest.cpp
    unit/common/UtilsTest.cpp
)

if(COMMON_TEST_SOURCES)
    add_executable(ClayEngineVK_CommonTests
        ${COMMON_TEST_SOURCES}
        ${TEST_COMMON_SOURCES}
    )
    
    target_link_libraries(ClayEngineVK_CommonTests
        PRIVATE
        ClayEngineVK
        gtest_main
        gmock_main
    )
    
    include(GoogleTest)
    gtest_discover_tests(ClayEngineVK_CommonTests
        PROPERTIES
        LABELS "common"
    )
endif()

# ==============================================================================
# Test Resources
# ==============================================================================

# Copy test resources if needed
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_resources)
