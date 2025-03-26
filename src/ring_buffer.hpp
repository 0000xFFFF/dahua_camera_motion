#pragma once

#include <array>
#include <atomic>
#include <optional>

template <typename T, size_t Size>
class LockFreeRingBuffer {
  private:
    std::array<T, Size> m_buffer;
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};

  public:
    bool push(T item)
    {
        size_t current_head = m_head.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % Size;

        if (next_head == m_tail.load(std::memory_order_acquire))
            return false; // Buffer full

        m_buffer[current_head] = std::move(item);
        m_head.store(next_head, std::memory_order_release);
        return true;
    }

    std::optional<T> pop()
    {
        if (m_head == m_tail)
            return std::nullopt; // Buffer empty

        T item = std::move(m_buffer[m_tail]);
        m_tail.store((m_tail + 1) % Size, std::memory_order_release);
        return item;
    }
};
