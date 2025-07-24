# Test executable
add_executable(ring_buffer_test
    ring_buffer_test.cpp
)

# Link against the ring buffer library and test framework
target_link_libraries(ring_buffer_test
    PRIVATE
        spsc::ring-buffer
        Catch2::Catch2WithMain
        Threads::Threads
)

# Set C++ standard
target_compile_features(ring_buffer_test PRIVATE cxx_std_17)

# Add compile options for better debugging and warnings
if(MSVC)
    target_compile_options(ring_buffer_test PRIVATE /W4)
else()
    target_compile_options(ring_buffer_test PRIVATE
        -Wall -Wextra -Wpedantic
        -g  # Debug info
        -O2 # Some optimization for realistic performance testing
    )
endif()

# Register tests with CTest
include(CTest)
include(Catch)
catch_discover_tests(ring_buffer_test)
