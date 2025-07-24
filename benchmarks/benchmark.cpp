/**
 * @file benchmark.cpp
 * @brief Standalone benchmark suite for SPSC ring buffer
 * @author Fadli Arsani <fadlialim0029@gmail.com>
 */

#include <lockfree/ring_buffer.hpp>
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <queue>
#include <mutex>
#include <string>

using namespace lockfree;

class BenchmarkTimer {
private:
    std::chrono::high_resolution_clock::time_point start_;
    
public:
    BenchmarkTimer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsedMs() const {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        return duration.count() / 1000.0;
    }
    
    double elapsedUs() const {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
        return duration.count() / 1000.0;
    }
};

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(100, '=') << std::endl;
}

void printResults(const std::string& test_name, double elapsed_ms, uint64_t operations) {
    double ops_per_sec = (operations * 1000.0) / elapsed_ms;
    double ns_per_op = (elapsed_ms * 1000000.0) / operations;
    
    std::cout << std::left << std::setw(25) << test_name << ": ";
    std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec";
    std::cout << std::setw(10) << std::fixed << std::setprecision(1) << ns_per_op << " ns/op";
    std::cout << std::setw(10) << std::fixed << std::setprecision(2) << elapsed_ms << " ms" << std::endl;
}

/**
 * Benchmark 1: Maximum Throughput Test
 */
void benchmarkMaxThroughput() {
    printSeparator("Maximum Throughput Benchmark");
    
    RingBuffer<uint64_t, 4096> buffer;
    constexpr auto TEST_DURATION = std::chrono::seconds(2);
    
    std::atomic<bool> running{true};
    std::atomic<uint64_t> pushed{0};
    std::atomic<uint64_t> popped{0};
    std::atomic<uint64_t> push_failures{0};
    
    BenchmarkTimer timer;
    
    // Producer thread
    std::thread producer([&]() {
        uint64_t counter = 0;
        while (running.load(std::memory_order_relaxed)) {
            if (buffer.try_push(counter)) {
                pushed.fetch_add(1, std::memory_order_relaxed);
                counter++;
            } else {
                push_failures.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (running.load(std::memory_order_relaxed)) {
            if (auto item = buffer.try_pop()) {
                popped.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        
        // Drain remaining items
        while (auto item = buffer.try_pop()) {
            popped.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    // Let it run
    std::this_thread::sleep_for(TEST_DURATION);
    running = false;
    
    producer.join();
    consumer.join();
    
    double elapsed_ms = timer.elapsedMs();
    uint64_t total_pushed = pushed.load();
    uint64_t total_popped = popped.load();
    uint64_t failures = push_failures.load();
    
    std::cout << "Test Duration    : " << std::fixed << std::setprecision(1) << elapsed_ms << " ms" << std::endl;
    std::cout << "Items Pushed     : " << total_pushed << std::endl;
    std::cout << "Items Popped     : " << total_popped << std::endl;
    std::cout << "Push Failures    : " << failures << " (" << std::fixed << std::setprecision(3) 
              << (failures * 100.0 / (total_pushed + failures)) << "%)" << std::endl;
    std::cout << "Data Loss        : " << (total_pushed - total_popped) << " items" << std::endl;
    
    printResults("Push Throughput", elapsed_ms, total_pushed);
    printResults("Pop Throughput", elapsed_ms, total_popped);
    printResults("Combined Throughput", elapsed_ms, total_pushed + total_popped);
}

/**
 * Benchmark 2: Latency Test (Single Operation Timing)
 */
void benchmarkLatency() {
    printSeparator("Latency Benchmark (Single Operations)");
    
    RingBuffer<int, 1024> buffer;
    constexpr int NUM_SAMPLES = 1000000;
    
    // Warm up
    for (int i = 0; i < 1000; ++i) {
        (void)buffer.try_push(i);
        (void)buffer.try_pop();
    }
    
    // Push latency
    BenchmarkTimer push_timer;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        (void)buffer.try_push(i);
    }
    double push_elapsed = push_timer.elapsedUs();
    
    // Pop latency  
    BenchmarkTimer pop_timer;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        (void)buffer.try_pop();
    }
    double pop_elapsed = pop_timer.elapsedUs();
    
    std::cout << "Push Latency     : " << std::fixed << std::setprecision(2) 
              << (push_elapsed / NUM_SAMPLES) << " ns/op" << std::endl;
    std::cout << "Pop Latency      : " << std::fixed << std::setprecision(2) 
              << (pop_elapsed / NUM_SAMPLES) << " ns/op" << std::endl;
    std::cout << "Round-trip       : " << std::fixed << std::setprecision(2) 
              << ((push_elapsed + pop_elapsed) / NUM_SAMPLES) << " ns/op" << std::endl;
}

/**
 * Benchmark 3: Different Buffer Sizes
 */
void benchmarkBufferSizes() {
    printSeparator("Buffer Size Comparison");
    
    constexpr int NUM_OPERATIONS = 1000000;
    std::vector<size_t> sizes = {64, 256, 1024, 4096, 16384};
    
    std::cout << std::left << std::setw(15) << "Buffer Size" 
              << std::setw(15) << "Throughput" 
              << std::setw(15) << "ns/op" << std::endl;
    std::cout << std::string(45, '-') << std::endl;
    
    for (size_t size : sizes) {
        BenchmarkTimer timer;
        
        if (size == 64) {
            RingBuffer<int, 64> buffer;
            std::thread producer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_push(i)) std::this_thread::yield();
                }
            });
            std::thread consumer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_pop()) std::this_thread::yield();
                }
            });
            producer.join(); consumer.join();
        } else if (size == 256) {
            RingBuffer<int, 256> buffer;
            std::thread producer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_push(i)) std::this_thread::yield();
                }
            });
            std::thread consumer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_pop()) std::this_thread::yield();
                }
            });
            producer.join(); consumer.join();
        } else if (size == 1024) {
            RingBuffer<int, 1024> buffer;
            std::thread producer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_push(i)) std::this_thread::yield();
                }
            });
            std::thread consumer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_pop()) std::this_thread::yield();
                }
            });
            producer.join(); consumer.join();
        } else if (size == 4096) {
            RingBuffer<int, 4096> buffer;
            std::thread producer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_push(i)) std::this_thread::yield();
                }
            });
            std::thread consumer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_pop()) std::this_thread::yield();
                }
            });
            producer.join(); consumer.join();
        } else if (size == 16384) {
            RingBuffer<int, 16384> buffer;
            std::thread producer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_push(i)) std::this_thread::yield();
                }
            });
            std::thread consumer([&]() {
                for (int i = 0; i < NUM_OPERATIONS; ++i) {
                    while (!buffer.try_pop()) std::this_thread::yield();
                }
            });
            producer.join(); consumer.join();
        }
        
        double elapsed_ms = timer.elapsedMs();
        double ops_per_sec = (NUM_OPERATIONS * 2 * 1000.0) / elapsed_ms; // *2 for push+pop
        double ns_per_op = (elapsed_ms * 1000000.0) / (NUM_OPERATIONS * 2);
        
        std::cout << std::left << std::setw(15) << size 
                  << std::setw(15) << std::fixed << std::setprecision(0) << ops_per_sec
                  << std::setw(15) << std::fixed << std::setprecision(2) << ns_per_op << std::endl;
    }
}

/**
 * Benchmark 4: vs std::queue + mutex
 */
void benchmarkVsStdQueue() {
    printSeparator("Ring Buffer vs std::queue + mutex");
    
    constexpr int NUM_OPERATIONS = 500000;
    double ring_time = 0.0;
    double mutex_time = 0.0;
    
    // Test ring buffer
    {
        RingBuffer<int, 2048> buffer;
        BenchmarkTimer timer;
        
        std::thread producer([&]() {
            for (int i = 0; i < NUM_OPERATIONS; ++i) {
                while (!buffer.try_push(i)) {
                    std::this_thread::yield();
                }
            }
        });
        
        std::thread consumer([&]() {
            for (int i = 0; i < NUM_OPERATIONS; ++i) {
                while (!buffer.try_pop()) {
                    std::this_thread::yield();
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        ring_time = timer.elapsedMs();
        printResults("SPSC Ring Buffer", ring_time, NUM_OPERATIONS * 2);
    }
    
    // Test std::queue + mutex
    {
        std::queue<int> queue;
        std::mutex mutex;
        BenchmarkTimer timer;
        
        std::thread producer([&]() {
            for (int i = 0; i < NUM_OPERATIONS; ++i) {
                std::lock_guard<std::mutex> lock(mutex);
                queue.push(i);
            }
        });
        
        std::thread consumer([&]() {
            for (int i = 0; i < NUM_OPERATIONS; ++i) {
                std::unique_lock<std::mutex> lock(mutex);
                while (queue.empty()) {
                    lock.unlock();
                    std::this_thread::yield();
                    lock.lock();
                }
                queue.pop();
            }
        });
        
        producer.join();
        consumer.join();
        
        mutex_time = timer.elapsedMs();
        printResults("std::queue + mutex", mutex_time, NUM_OPERATIONS * 2);
    }
    
    // Calculate speedup
    double speedup = mutex_time / ring_time;
    std::cout << "\nSpeedup: " << std::fixed << std::setprecision(2) 
              << speedup << "x faster" << std::endl;
}

/**
 * Benchmark 5: Memory Usage Analysis
 */
void benchmarkMemoryUsage() {
    printSeparator("Memory Usage Analysis");
    
    std::cout << "Buffer Size Analysis:" << std::endl;
    std::cout << std::left << std::setw(15) << "Capacity" 
              << std::setw(15) << "Buffer Size" 
              << std::setw(15) << "Total Size" 
              << std::setw(15) << "Per Item" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    std::vector<size_t> capacities = {64, 256, 1024, 4096, 16384};
    
    for (size_t cap : capacities) {
        size_t buffer_size = cap * sizeof(int);
        size_t total_size = buffer_size + 2 * 64 + 64; // head + tail + buffer (cache-aligned)
        double per_item = (double)total_size / (cap - 1); // -1 for effective capacity
        
        std::cout << std::left << std::setw(15) << cap
                  << std::setw(15) << buffer_size << " bytes"
                  << std::setw(15) << total_size << " bytes"
                  << std::setw(15) << std::fixed << std::setprecision(2) << per_item << " bytes" << std::endl;
    }
}

int main() {
    std::cout << "SPSC Ring Buffer Benchmark Suite" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Compiler: " << __VERSION__ << std::endl;
    std::cout << "CPU Cores: " << std::thread::hardware_concurrency() << std::endl;
    
    benchmarkMaxThroughput();
    benchmarkLatency();
    benchmarkBufferSizes();
    benchmarkVsStdQueue();
    benchmarkMemoryUsage();
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "Benchmark Complete!" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    return 0;
}
