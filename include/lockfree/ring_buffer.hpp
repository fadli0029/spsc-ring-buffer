/**
 * @file ring_buffer.hpp
 * @brief High-performance lock-free single-producer single-consumer ring buffer
 * @author Fadli Arsani <fadlialim0029@gmail.com>
 * @version 1.0.0
 * @date July 2025
 *
 * @copyright MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <array>
#include <atomic>
#include <optional>
#include <type_traits>

namespace lockfree {

/**
 * @brief Lock-free single-producer single-consumer ring buffer
 *
 * A high-performance circular queue optimized for scenarios where exactly one thread
 * produces data and exactly one thread consumes data. Provides wait-free operations
 * with zero memory allocation after construction.
 *
 * Key features:
 * - Lock-free and wait-free operations
 * - Cache-optimized with 64-byte alignment
 * - Zero memory allocation after construction
 * - Type-safe with move semantics support
 * - Capacity must be a power of 2 for optimal performance
 * - Actual storage capacity is Capacity-1 items
 *
 * @tparam T The type of elements stored in the ring buffer
 * @tparam Capacity The maximum number of elements (must be power of 2)
 *
 * @note This implementation keeps one slot empty to distinguish between
 *       empty and full states, so effective capacity is Capacity-1.
 *
 * @warning This class is NOT thread-safe for multiple producers or consumers.
 *          Use appropriate synchronization or consider MPSC variants for such cases.
 *
 * Example usage:
 * @code
 * #include <lockfree/ring_buffer.hpp>
 *
 * lockfree::RingBuffer<int, 1024> buffer;
 *
 * // Producer thread
 * if (buffer.try_push(42)) {
 *     // Success
 * }
 *
 * // Consumer thread
 * if (auto item = buffer.try_pop()) {
 *     // Process *item
 * }
 * @endcode
 */
template <typename T, std::size_t Capacity>
class RingBuffer {
    static_assert(std::is_move_constructible_v<T>,
                  "T must be move constructible");
    static_assert(std::is_move_assignable_v<T>,
                  "T must be move assignable");
    static_assert((Capacity & (Capacity - 1)) == 0 && Capacity > 1,
                  "Capacity must be a power of 2 and greater than 1");

private:
    static constexpr std::size_t kIndexMask = Capacity - 1;

    // Separate cache lines to prevent false sharing between producer and consumer
    alignas(64) std::atomic<std::size_t> head_{0};  ///< Consumer index
    alignas(64) std::atomic<std::size_t> tail_{0};  ///< Producer index

    // Data storage aligned to cache line boundary
    alignas(64) std::array<T, Capacity> buffer_;

public:
    /// The type of elements stored in the buffer
    using value_type = T;
    /// The size type used for indices and sizes
    using size_type = std::size_t;

    /**
     * @brief Default constructor
     *
     * Constructs an empty ring buffer. All elements in the internal storage
     * are default-constructed.
     */
    RingBuffer() = default;

    /**
     * @brief Destructor
     *
     * Destroys the ring buffer. Note that this does not call destructors
     * on remaining elements in the buffer. Users should drain the buffer
     * before destruction if needed.
     */
    ~RingBuffer() = default;

    // Non-copyable and non-movable for safety
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    /**
     * @brief Attempt to push an element (copy version)
     *
     * Tries to add an element to the ring buffer. This operation is wait-free
     * for the producer thread.
     *
     * @param item The element to add to the buffer
     * @return true if the element was successfully added, false if buffer is full
     *
     * @note This function should only be called from the producer thread
     */
    [[nodiscard]] bool try_push(const T& item) noexcept(std::is_nothrow_copy_assignable_v<T>) {
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto next_tail = (current_tail + 1) & kIndexMask;

        // Check if buffer is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        // Store the item
        buffer_[current_tail] = item;

        // Publish the new tail position
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Attempt to push an element (move version)
     *
     * Tries to add an element to the ring buffer using move semantics.
     * This operation is wait-free for the producer thread.
     *
     * @param item The element to move into the buffer
     * @return true if the element was successfully added, false if buffer is full
     *
     * @note This function should only be called from the producer thread
     */
    [[nodiscard]] bool try_push(T&& item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto next_tail = (current_tail + 1) & kIndexMask;

        // Check if buffer is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        // Move the item
        buffer_[current_tail] = std::move(item);

        // Publish the new tail position
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Attempt to pop an element
     *
     * Tries to remove and return an element from the ring buffer.
     * This operation is wait-free for the consumer thread.
     *
     * @return std::optional containing the popped element if successful,
     *         std::nullopt if buffer is empty
     *
     * @note This function should only be called from the consumer thread
     */
    [[nodiscard]] std::optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        const auto current_head = head_.load(std::memory_order_relaxed);

        // Check if buffer is empty
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        // Move the item out of the buffer
        T item = std::move(buffer_[current_head]);

        // Publish the new head position
        head_.store((current_head + 1) & kIndexMask, std::memory_order_release);
        return item;
    }

    /**
     * @brief Check if the buffer appears empty
     *
     * @return true if the buffer appears empty at the time of the call
     *
     * @note This is an approximate check. The state may change immediately
     *       after this function returns due to concurrent operations.
     */
    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) ==
               tail_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Check if the buffer appears full
     *
     * @return true if the buffer appears full at the time of the call
     *
     * @note This is an approximate check. The state may change immediately
     *       after this function returns due to concurrent operations.
     */
    [[nodiscard]] bool full() const noexcept {
        const auto next_tail = (tail_.load(std::memory_order_relaxed) + 1) & kIndexMask;
        return next_tail == head_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get the approximate current size
     *
     * @return The approximate number of elements currently in the buffer
     *
     * @note This is an approximate value. The actual size may change
     *       immediately after this function returns due to concurrent operations.
     */
    [[nodiscard]] size_type size() const noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_relaxed);
        return (tail - head) & kIndexMask;
    }

    /**
     * @brief Get the maximum capacity
     *
     * @return The maximum number of elements this buffer can hold
     *
     * @note Due to the implementation keeping one slot empty, this returns
     *       Capacity - 1, which is the actual usable capacity.
     */
    [[nodiscard]] static constexpr size_type capacity() noexcept {
        return Capacity - 1;  // One slot kept empty for empty/full distinction
    }

    /**
     * @brief Get the total buffer size
     *
     * @return The total number of slots in the internal buffer
     *
     * @note This is primarily for debugging/testing purposes.
     */
    [[nodiscard]] static constexpr size_type buffer_size() noexcept {
        return Capacity;
    }
};

} // namespace lockfree
