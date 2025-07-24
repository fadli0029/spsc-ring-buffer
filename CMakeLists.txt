cmake_minimum_required(VERSION 3.14)
project(spsc-ring-buffer
    VERSION 1.0.0
    DESCRIPTION "High-performance lock-free SPSC ring buffer"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compile options
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    add_compile_options(-O3 -march=native)  # Optimize for target machine
endif()

# Header-only library target
add_library(spsc-ring-buffer INTERFACE)
add_library(spsc::ring-buffer ALIAS spsc-ring-buffer)

# Set include directories
target_include_directories(spsc-ring-buffer
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Require C++17
target_compile_features(spsc-ring-buffer
    INTERFACE
        cxx_std_17
)

# Find threads (required for atomic operations)
find_package(Threads REQUIRED)
target_link_libraries(spsc-ring-buffer
    INTERFACE
        Threads::Threads
)

# Installation
include(GNUInstallDirs)
install(
    TARGETS spsc-ring-buffer
    EXPORT spsc-ring-buffer-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT spsc-ring-buffer-targets
    FILE spsc-ring-buffer-targets.cmake
    NAMESPACE spsc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spsc-ring-buffer
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    spsc-ring-buffer-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    cmake/spsc-ring-buffer-config.cmake.in
    spsc-ring-buffer-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spsc-ring-buffer
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/spsc-ring-buffer-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/spsc-ring-buffer-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spsc-ring-buffer
)

# Enable testing if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    option(BUILD_TESTING "Build tests" ON)
    option(BUILD_BENCHMARKS "Build benchmarks" ON)

    if(BUILD_TESTING AND NOT TARGET Catch2::Catch2)
        # Download and configure Catch2 if not found
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.4.0
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        enable_testing()
        add_subdirectory(tests)
    endif()

    if(BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
        add_subdirectory(benchmarks)
    endif()
endif()
