#pragma once

#include <array>
#include <atomic>
#include <opencv2/opencv.hpp>
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

class DoubleBufferMat {
  private:
    cv::Mat buffers[2];
    std::atomic<int> activeBuffer{0}; // read buffer

  public:
    DoubleBufferMat() {}
    DoubleBufferMat(cv::Mat initialData)
    {
        buffers[0] = initialData.clone();
        buffers[1] = initialData.clone();
    }

    // Writer updates the non-active buffer and then swaps
    void update(const cv::Mat& frame)
    {
        int writeIndex = 1 - activeBuffer.load(std::memory_order_acquire);
        buffers[writeIndex] = frame.clone();
        activeBuffer.store(writeIndex, std::memory_order_release);
    }

    // Reader reads from the active buffer
    cv::Mat get() const
    {
        int readIndex = activeBuffer.load(std::memory_order_acquire);
        return buffers[readIndex].clone(); // Return a copy
    }
};

class DoubleBufferList {
  private:
    std::list<int> buffers[2];
    std::atomic<int> activeBuffer{0}; // read buffer

  public:
    DoubleBufferList() {}
    DoubleBufferList(std::list<int> initialData)
    {
        buffers[0] = initialData;
        buffers[1] = initialData;
    }

    // Writer updates the non-active buffer and then swaps
    void update(const std::list<int>& newData)
    {
        int writeIndex = 1 - activeBuffer.load(std::memory_order_acquire);
        buffers[writeIndex] = newData;
        activeBuffer.store(writeIndex, std::memory_order_release);
    }

    // Reader reads from the active buffer
    std::list<int> get() const
    {
        int readIndex = activeBuffer.load(std::memory_order_acquire);
        return buffers[readIndex]; // Return a copy
    }
};
