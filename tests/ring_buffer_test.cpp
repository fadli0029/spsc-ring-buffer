/**
 * @file ring_buffer_test.cpp
 * @brief Comprehensive test suite for the SPSC ring buffer
 * @author Fadli Arsani <fadlialim0029@gmail.com>
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <lockfree/ring_buffer.hpp>

#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>

using namespace lockfree;

// Test fixture for common test data
struct TestMessage {
    int id;
    std::string data;
    
    TestMessage() = default;
    TestMessage(int i, std::string d) : id(i), data(std::move(d)) {}
    
    bool operator==(const TestMessage& other) const {
        return id == other.id && data == other.data;
    }
};

TEST_CASE("Ring Buffer Basic Operations", "[basic]") {
    RingBuffer<int, 8> buffer;
    
    SECTION("Initial state") {
        REQUIRE(buffer.empty());
        REQUIRE_FALSE(buffer.full());
        REQUIRE(buffer.size() == 0);
        REQUIRE(buffer.capacity() == 7);  // Capacity - 1
    }
    
    SECTION("Single push and pop") {
        REQUIRE(buffer.try_push(42));
        REQUIRE_FALSE(buffer.empty());
        REQUIRE(buffer.size() == 1);
        
        auto item = buffer.try_pop();
        REQUIRE(item.has_value());
        REQUIRE(*item == 42);
        REQUIRE(buffer.empty());
        REQUIRE(buffer.size() == 0);
    }
    
    SECTION("Pop from empty buffer") {
        auto item = buffer.try_pop();
        REQUIRE_FALSE(item.has_value());
    }
    
    SECTION("Fill to capacity") {
        // Fill buffer to capacity (7 items)
        for (int i = 0; i < 7; ++i) {
            REQUIRE(buffer.try_push(i));
        }
        
        REQUIRE(buffer.full());
        REQUIRE(buffer.size() == 7);
        
        // Should fail to push when full
        REQUIRE_FALSE(buffer.try_push(999));
        
        // Pop all items and verify order
        for (int i = 0; i < 7; ++i) {
            auto item = buffer.try_pop();
            REQUIRE(item.has_value());
            REQUIRE(*item == i);
        }
        
        REQUIRE(buffer.empty());
    }
}

TEST_CASE("Ring Buffer Move Semantics", "[move]") {
    RingBuffer<TestMessage, 16> buffer;
    
    SECTION("Move construction") {
        TestMessage msg(1, "Hello World");
        REQUIRE(buffer.try_push(std::move(msg)));
        
        auto retrieved = buffer.try_pop();
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->id == 1);
        REQUIRE(retrieved->data == "Hello World");
    }
    
    SECTION("Large objects efficiency") {
        // Test with large string to ensure move semantics work
        std::string large_data(10000, 'A');
        TestMessage large_msg(42, std::move(large_data));
        
        REQUIRE(buffer.try_push(std::move(large_msg)));
        
        auto retrieved = buffer.try_pop();
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->id == 42);
        REQUIRE(retrieved->data.size() == 10000);
        REQUIRE(retrieved->data[0] == 'A');
    }
}

TEST_CASE("Ring Buffer Wrap-Around", "[wraparound]") {
    RingBuffer<int, 8> buffer;  // Capacity of 7
    
    SECTION("Multiple cycles") {
        constexpr int NUM_CYCLES = 100;
        constexpr int ITEMS_PER_CYCLE = 5;
        
        for (int cycle = 0; cycle < NUM_CYCLES; ++cycle) {
            // Fill buffer partially
            for (int i = 0; i < ITEMS_PER_CYCLE; ++i) {
                int value = cycle * ITEMS_PER_CYCLE + i;
                REQUIRE(buffer.try_push(value));
            }
            
            // Pop all items and verify order
            for (int i = 0; i < ITEMS_PER_CYCLE; ++i) {
                auto item = buffer.try_pop();
                int expected = cycle * ITEMS_PER_CYCLE + i;
                REQUIRE(item.has_value());
                REQUIRE(*item == expected);
            }
            
            REQUIRE(buffer.empty());
        }
    }
}

TEST_CASE("SPSC Correctness", "[spsc][threading]") {
    RingBuffer<int, 1024> buffer;
    constexpr int NUM_ITEMS = 50000;
    
    std::atomic<bool> producer_done{false};
    std::atomic<int> items_sent{0};
    std::atomic<int> items_received{0};
    std::vector<int> received_items;
    received_items.reserve(NUM_ITEMS);
    
    SECTION("Single producer single consumer") {
        // Producer thread
        std::thread producer([&]() {
            for (int i = 0; i < NUM_ITEMS; ++i) {
                while (!buffer.try_push(i)) {
                    std::this_thread::yield();
                }
                items_sent.fetch_add(1, std::memory_order_relaxed);
            }
            producer_done.store(true, std::memory_order_release);
        });
        
        // Consumer thread
        std::thread consumer([&]() {
            int received = 0;
            while (received < NUM_ITEMS) {
                auto item = buffer.try_pop();
                if (item) {
                    received_items.push_back(*item);
                    items_received.fetch_add(1, std::memory_order_relaxed);
                    received++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        // Verify results
        REQUIRE(items_sent.load() == NUM_ITEMS);
        REQUIRE(items_received.load() == NUM_ITEMS);
        REQUIRE(received_items.size() == NUM_ITEMS);
        
        // Verify order
        for (int i = 0; i < NUM_ITEMS; ++i) {
            REQUIRE(received_items[i] == i);
        }
    }
}

TEST_CASE("High Frequency Stress Test", "[stress][threading]") {
    RingBuffer<uint64_t, 2048> buffer;
    constexpr auto TEST_DURATION = std::chrono::milliseconds(500);  // Shorter for unit tests
    
    std::atomic<bool> test_running{true};
    std::atomic<uint64_t> total_pushed{0};
    std::atomic<uint64_t> total_popped{0};
    std::atomic<uint64_t> push_failures{0};
    
    SECTION("Maximum throughput test") {
        auto start_time = std::chrono::steady_clock::now();
        
        // Producer thread
        std::thread producer([&]() {
            uint64_t counter = 0;
            while (test_running.load(std::memory_order_relaxed)) {
                if (buffer.try_push(counter)) {
                    total_pushed.fetch_add(1, std::memory_order_relaxed);
                    counter++;
                } else {
                    push_failures.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::yield();
                }
            }
        });
        
        // Consumer thread
        std::thread consumer([&]() {
            while (test_running.load(std::memory_order_relaxed)) {
                auto item = buffer.try_pop();
                if (item) {
                    total_popped.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
            
            // Drain remaining items
            while (auto item = buffer.try_pop()) {
                total_popped.fetch_add(1, std::memory_order_relaxed);
            }
        });
        
        // Let it run for test duration
        std::this_thread::sleep_for(TEST_DURATION);
        test_running.store(false, std::memory_order_release);
        
        producer.join();
        consumer.join();
        
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        uint64_t pushed = total_pushed.load();
        uint64_t popped = total_popped.load();
        uint64_t failures = push_failures.load();
        
        // Verify no data loss
        REQUIRE(pushed == popped);
        
        // Verify reasonable throughput (at least 100K ops/sec)
        double throughput = (pushed * 1000.0) / elapsed_ms;
        REQUIRE(throughput > 100000);
        
        // Verify low failure rate (< 1%)
        if (pushed > 0) {
            double failure_rate = (double)failures / (pushed + failures);
            REQUIRE(failure_rate < 0.01);
        }
        
        INFO("Throughput: " << throughput << " ops/sec");
        INFO("Push failures: " << failures << " (" << (failures * 100.0 / (pushed + failures)) << "%)");
    }
}

TEST_CASE("Memory Ordering Validation", "[memory][threading]") {
    RingBuffer<std::pair<int, uint64_t>, 256> buffer;
    constexpr int NUM_BATCHES = 1000;
    constexpr int BATCH_SIZE = 20;
    
    std::atomic<bool> error_detected{false};
    
    SECTION("Memory ordering stress test") {
        std::thread worker([&]() {
            for (int batch = 0; batch < NUM_BATCHES && !error_detected.load(); ++batch) {
                std::vector<std::pair<int, uint64_t>> sent_items;
                
                // Push a batch
                for (int i = 0; i < BATCH_SIZE; ++i) {
                    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
                    std::pair<int, uint64_t> item{batch * BATCH_SIZE + i, static_cast<uint64_t>(timestamp)};
                    
                    while (!buffer.try_push(item) && !error_detected.load()) {
                        std::this_thread::yield();
                    }
                    sent_items.push_back(item);
                }
                
                // Pop the batch and verify
                for (const auto& expected : sent_items) {
                    std::optional<std::pair<int, uint64_t>> item;
                    while (!(item = buffer.try_pop()) && !error_detected.load()) {
                        std::this_thread::yield();
                    }
                    
                    if (!item || item->first != expected.first) {
                        error_detected.store(true);
                        FAIL("Memory ordering violation detected");
                        return;
                    }
                }
            }
        });
        
        worker.join();
        REQUIRE_FALSE(error_detected.load());
    }
}

TEST_CASE("Edge Cases", "[edge]") {
    SECTION("Power of 2 requirement") {
        // These should compile fine
        RingBuffer<int, 2> buffer2;
        RingBuffer<int, 4> buffer4;
        RingBuffer<int, 8> buffer8;
        RingBuffer<int, 1024> buffer1024;
        
        REQUIRE(buffer2.capacity() == 1);
        REQUIRE(buffer4.capacity() == 3);
        REQUIRE(buffer8.capacity() == 7);
        REQUIRE(buffer1024.capacity() == 1023);
    }
    
    SECTION("Minimum capacity buffer") {
        RingBuffer<int, 2> tiny_buffer;  // Capacity of 1
        
        REQUIRE(tiny_buffer.empty());
        REQUIRE_FALSE(tiny_buffer.full());
        REQUIRE(tiny_buffer.capacity() == 1);
        
        // Can push one item
        REQUIRE(tiny_buffer.try_push(42));
        REQUIRE(tiny_buffer.full());
        REQUIRE(tiny_buffer.size() == 1);
        
        // Cannot push another
        REQUIRE_FALSE(tiny_buffer.try_push(43));
        
        // Can pop the item
        auto item = tiny_buffer.try_pop();
        REQUIRE(item.has_value());
        REQUIRE(*item == 42);
        REQUIRE(tiny_buffer.empty());
    }
}

// Benchmark tests (optional, runs with --benchmark flag)
TEST_CASE("Performance Benchmarks", "[benchmark]") {
    RingBuffer<int, 1024> buffer;
    
    BENCHMARK("Push operation") {
        static int counter = 0;
        bool result = buffer.try_push(counter++);
        return result;
    };
    
    // Setup for pop benchmark
    for (int i = 0; i < 500; ++i) {
        (void)buffer.try_push(i);  // Cast to void to suppress nodiscard warning
    }
    
    BENCHMARK("Pop operation") {
        return buffer.try_pop();
    };
}

TEST_CASE("Comparison with std::queue", "[comparison][benchmark]") {
    constexpr int NUM_OPERATIONS = 10000;
    
    SECTION("Lock-free ring buffer performance") {
        RingBuffer<int, 2048> buffer;
        
        auto start = std::chrono::high_resolution_clock::now();
        
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
        
        auto end = std::chrono::high_resolution_clock::now();
        auto ring_buffer_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        INFO("Ring buffer time: " << ring_buffer_time.count() << " μs");
        
        // Test std::queue with mutex for comparison
        std::queue<int> std_queue;
        std::mutex queue_mutex;
        
        start = std::chrono::high_resolution_clock::now();
        
        std::thread mutex_producer([&]() {
            for (int i = 0; i < NUM_OPERATIONS; ++i) {
                std::lock_guard<std::mutex> lock(queue_mutex);
                std_queue.push(i);
            }
        });
        
        std::thread mutex_consumer([&]() {
            for (int i = 0; i < NUM_OPERATIONS; ++i) {
                std::unique_lock<std::mutex> lock(queue_mutex);
                while (std_queue.empty()) {
                    lock.unlock();
                    std::this_thread::yield();
                    lock.lock();
                }
                std_queue.pop();
            }
        });
        
        mutex_producer.join();
        mutex_consumer.join();
        
        end = std::chrono::high_resolution_clock::now();
        auto mutex_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        INFO("Mutex queue time: " << mutex_time.count() << " μs");
        INFO("Speedup: " << (double)mutex_time.count() / ring_buffer_time.count() << "x");
        
        // In CI environments or under certain conditions, the lock-free implementation
        // might not show performance benefits due to virtualization, limited cores, etc.
#ifdef _MSC_VER
        char* ci_env = nullptr;
        size_t len = 0;
        bool is_ci = (_dupenv_s(&ci_env, &len, "CI") == 0 && ci_env != nullptr);
        if (ci_env) free(ci_env);
        if (is_ci) {
#else
        if (std::getenv("CI")) {
#endif
            // In CI, we only check that the implementation works correctly
            // Performance can vary significantly in virtualized environments
            INFO("Performance comparison skipped in CI environment");
            INFO("Lock-free implementation may not show benefits in virtualized/CI environments");
            SUCCEED("Performance test completed - results logged for information only");
        } else {
            // On real hardware, lock-free should typically be faster
            REQUIRE(ring_buffer_time < mutex_time);
        }
    }
}
