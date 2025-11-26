#include "buffers.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <opencv2/core/ocl.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

// UMat-based double buffer
class DoubleBufferUMat {
  public:
    void update(const cv::UMat& mat)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        mat.copyTo(m_buffer);
    }
    cv::UMat get()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_buffer.clone();
    }

  private:
    cv::UMat m_buffer;
    std::mutex m_mtx;
};

class FrameReader {
  public:
    FrameReader(int channel,
                const std::string& ip,
                const std::string& username,
                const std::string& password,
                int subtype,
                bool autostart,
                bool has_placeholder);

    cv::UMat get_latest_frame(bool no_empty_frame = false);
    void disable_sleep();
    void enable_sleep();
    double get_fps();
    void start();
    void stop();
    bool is_running();
    bool is_active();

  private:
    void connect_and_read();
    std::string construct_rtsp_url(const std::string& ip, const std::string& username, const std::string& password, int subtype);
    void put_placeholder();

  private:
    std::thread m_thread;
    std::string m_ip;
    std::string m_username;
    std::string m_password;
    int m_channel;
    int m_subtype;
    std::atomic<double> captured_fps{15.0};

    std::atomic<bool> m_sleep{true};
    std::mutex m_mtx;
    std::condition_variable m_cv;

    LockFreeRingBuffer<cv::UMat, 2> m_frame_buffer;
    DoubleBufferUMat m_frame_dbuffer;
    cv::VideoCapture m_cap;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cleaning{false};
    std::atomic<bool> m_active{false};
};
