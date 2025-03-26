#include <atomic>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <thread>

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

class FrameReader {
  public:
    FrameReader(int ch, const std::string& ip,
                const std::string& username, const std::string& password);

    cv::Mat get_latest_frame();
    void stop();

  private:
    void connect_and_read();
    std::string construct_rtsp_url(const std::string& ip, const std::string& username,
                                   const std::string& password);

  private:
    std::thread m_thread;
    std::string m_ip;
    std::string m_username;
    std::string m_password;
    int m_channel;
    LockFreeRingBuffer<cv::Mat, 2> m_frame_buffer;
    cv::VideoCapture m_cap;
    std::atomic<bool> m_running{true};
    std::mutex m_mtx;
};
