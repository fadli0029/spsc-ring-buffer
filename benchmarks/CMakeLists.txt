# Benchmark executable
add_executable(benchmark
    benchmark.cpp
)

# Link against the ring buffer library
target_link_libraries(benchmark
    PRIVATE
        spsc::ring-buffer
        Threads::Threads
)

# Set C++ standard
target_compile_features(benchmark PRIVATE cxx_std_17)

# Add compile options for optimal performance
if(MSVC)
    target_compile_options(benchmark PRIVATE /O2 /DNDEBUG)
else()
    target_compile_options(benchmark PRIVATE 
        -O3          # Maximum optimization
        -march=native # Optimize for current CPU
        -DNDEBUG     # Disable assertions
        -flto        # Link-time optimization
    )
endif()
