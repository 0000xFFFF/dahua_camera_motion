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
        size_t current_tail = m_tail.load(std::memory_order_relaxed);
        size_t current_head = m_head.load(std::memory_order_acquire);

        if (current_head == current_tail)
            return std::nullopt; // Buffer empty

        T item = std::move(m_buffer[current_tail]);
        m_tail.store((current_tail + 1) % Size, std::memory_order_release);
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

class DoubleBufferVec {
  private:
    std::vector<int> buffers[2];
    std::atomic<int> activeBuffer{0}; // read buffer

  public:
    DoubleBufferVec() {}
    DoubleBufferVec(std::vector<int> initialData)
    {
        buffers[0] = initialData;
        buffers[1] = initialData;
    }

    // Writer updates the non-active buffer and then swaps
    void update(const std::vector<int>& newData)
    {
        int writeIndex = 1 - activeBuffer.load(std::memory_order_acquire);
        buffers[writeIndex] = newData;
        activeBuffer.store(writeIndex, std::memory_order_release);
    }

    // Reader reads from the active buffer
    std::vector<int> get() const
    {
        int readIndex = activeBuffer.load(std::memory_order_acquire);
        return buffers[readIndex]; // Return a copy
    }
};

template <typename T>
class DoubleBuffer {
  private:
    std::array<T, 2> buffers;
    std::atomic<int> activeBuffer{0}; // read buffer

  public:
    DoubleBuffer() = default;

    explicit DoubleBuffer(const T& initialData)
    {
        buffers[0] = cloneData(initialData);
        buffers[1] = cloneData(initialData);
    }

    void update(const T& data)
    {
        int writeIndex = 1 - activeBuffer.load(std::memory_order_acquire);
        buffers[writeIndex] = cloneData(data);
        activeBuffer.store(writeIndex, std::memory_order_release);
    }

    T get() const
    {
        int readIndex = activeBuffer.load(std::memory_order_acquire);
        return cloneData(buffers[readIndex]);
    }

  private:
    T cloneData(const T& data) const
    {
        return T(data);
    }
};
