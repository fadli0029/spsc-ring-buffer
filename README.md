# SPSC Ring Buffer

[![CI](https://github.com/fadli0029/spsc-ring-buffer/workflows/CI/badge.svg)](https://github.com/fadli0029/spsc-ring-buffer/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)

A high-performance, header-only, lock-free single-producer single-consumer (SPSC) ring buffer implementation in C++17.

## Performance

- **20+ million operations per second** on modern hardware
- **Zero memory allocation** after construction
- **Wait-free operations** for both producer and consumer
- **Cache-optimized** with 64-byte alignment to prevent false sharing
- **30x faster** than mutex-based alternatives

## Features

- **Header-only**: Just include `<lockfree/ring_buffer.hpp>`
- **Type-safe**: Full C++ type system support with move semantics
- **Modern C++17**: Uses `std::optional`, `constexpr`, and proper noexcept specifications
- **CMake integration**: Easy to integrate into existing projects

## Quick Start

### Installation

#### Option 1: Header-only (Simplest)
```bash
# Just copy the header file
curl -O https://raw.githubusercontent.com/fadli0029/spsc-ring-buffer/main/include/lockfree/ring_buffer.hpp
```

#### Option 2: CMake FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(
  spsc-ring-buffer
  GIT_REPOSITORY https://github.com/fadli0029/spsc-ring-buffer.git
  GIT_TAG main
)
FetchContent_MakeAvailable(spsc-ring-buffer)

target_link_libraries(your_target PRIVATE spsc::ring-buffer)
```

#### Option 3: System Installation
```bash
git clone https://github.com/fadli0029/spsc-ring-buffer.git
cd spsc-ring-buffer
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make install
```

### Basic Usage

```cpp
#include <lockfree/ring_buffer.hpp>
#include <thread>

int main() {
    // Create a ring buffer with 1024 capacity (power of 2 required)
    lockfree::RingBuffer<int, 1024> buffer;
    
    // Producer thread
    std::thread producer([&buffer]() {
        for (int i = 0; i < 1000; ++i) {
            while (!buffer.try_push(i)) {
                // Buffer full, retry
                std::this_thread::yield();
            }
        }
    });
    
    // Consumer thread
    std::thread consumer([&buffer]() {
        for (int i = 0; i < 1000; ++i) {
            std::optional<int> item;
            while (!(item = buffer.try_pop())) {
                // Buffer empty, retry
                std::this_thread::yield();
            }
            // Process *item
        }
    });
    
    producer.join();
    consumer.join();
    return 0;
}
```

## API Reference

### Core Operations

```cpp
// Try to push an element (copy)
bool try_push(const T& item);

// Try to push an element (move)
bool try_push(T&& item);

// Try to pop an element
std::optional<T> try_pop();
```

### Status Queries

```cpp
// Check if buffer appears empty
bool empty() const;

// Check if buffer appears full  
bool full() const;

// Get approximate current size
size_t size() const;

// Get maximum capacity (Capacity - 1)
static constexpr size_t capacity();
```

## Benchmarks

Performance on modern hardware (your results may vary):

| Metric | Result |
|--------|--------|
| **Throughput** | 20M ops/sec (combined) |
| **Single operation** | ~1.5ns per operation |
| **vs std::queue+mutex** | 30x faster |
| **Push failure rate** | 0.065% under stress |

### Run Your Own Benchmarks

```bash
git clone https://github.com/fadli0029/spsc-ring-buffer.git
cd spsc-ring-buffer
mkdir build && cd build
cmake .. -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
make
./benchmarks/benchmark
```

The benchmark suite includes:
- **Maximum throughput** testing
- **Single operation latency** measurement
- **Buffer size** impact analysis
- **Direct comparison** with std::queue + mutex
- **Memory usage** analysis

## Requirements

- **C++17** or later
- **Power-of-2 capacity** (enforced at compile time)
- **Single producer, single consumer** only

## Testing

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON
make
ctest
```

### Test Coverage

The comprehensive test suite includes:
- **Basic functionality** tests
- **SPSC correctness** (50K items transferred)
- **High-frequency stress** testing (20M+ ops/sec)
- **Memory ordering** validation
- **Wrap-around correctness**
- **Move semantics** verification
- **Edge cases** and error conditions

### Continuous Integration

Automatically tested on:
- **Linux** (GCC 12, Clang 15)
- **Windows** (MSVC)
- **macOS** (Clang)

With comprehensive validation:
- **AddressSanitizer** - Memory error detection
- **ThreadSanitizer** - Race condition detection  
- **UndefinedBehaviorSanitizer** - UB detection
- **Valgrind** - Memory leak detection
- **Code coverage** reporting

## Use Cases

Perfect for:
- **High-frequency sensor data** streaming
- **Audio/video processing** pipelines  
- **Game engine** component communication
- **Financial trading** systems
- **Real-time control** systems
- **Inter-thread messaging** with predictable latency

## ⚠️ Important Notes

- **Capacity must be power of 2** (enforced at compile time)
- **Actual storage capacity is Capacity-1** (one slot kept empty)
- **Single producer/consumer only** - not thread-safe for multiple producers
- **No blocking operations** - always returns immediately

## Implementation Details

- Uses **atomic head/tail indices** with appropriate memory ordering
- **64-byte cache line alignment** prevents false sharing
- **Power-of-2 masking** for fast modulo operations
- **Release-acquire semantics** ensure proper memory synchronization
- **Move semantics** support for efficient large object handling

## Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass (including CI)
5. Submit a pull request

### Development Setup

```bash
git clone https://github.com/fadli0029/spsc-ring-buffer.git
cd spsc-ring-buffer
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON -DBUILD_BENCHMARKS=ON
make
```

## License

MIT License - see [LICENSE](LICENSE) file for details.

## GitHub Topics

`cpp` `cpp17` `lockfree` `spsc` `ring-buffer` `concurrent` `atomic` `high-performance` `real-time` `embedded` `robotics` `trading` `audio` `gaming` `zero-allocation` `wait-free` `cache-optimized` `header-only` `cmake`
