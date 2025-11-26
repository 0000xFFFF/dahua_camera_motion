#include "buffers.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <opencv2/core/ocl.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

class FrameReader {
  public:
    FrameReader(int channel,
                const std::string& ip,
                const std::string& username,
                const std::string& password,
                int subtype,
                bool autostart,
                bool has_placeholder);

    cv::Mat get_latest_frame(bool no_empty_frame = false);
    cv::UMat get_latest_frame_umat(bool no_empty_frame = false); // Add this
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

    LockFreeRingBuffer<cv::Mat, 2> m_frame_buffer;
    DoubleBufferMat m_frame_dbuffer;
    cv::VideoCapture m_cap;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cleaning{false};
    std::atomic<bool> m_active{false};
};
