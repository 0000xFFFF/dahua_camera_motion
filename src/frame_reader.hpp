#include "ring_buffer.hpp"
#include <atomic>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

class FrameReader {
  public:
    FrameReader(int ch, const std::string& ip,
                const std::string& username, const std::string& password);

    cv::Mat get_latest_frame();
    void get_latest_frame_no_sleep();
    void get_latest_frame_sleep();
    double get_fps();
    void stop();

  private:
    void connect_and_read();
    std::string construct_rtsp_url(const std::string& ip, const std::string& username, const std::string& password);
    void put_placeholder();

  private:
    std::thread m_thread;
    std::string m_ip;
    std::string m_username;
    std::string m_password;
    int m_channel;
    std::atomic<double> captured_fps{30.0};

    std::atomic<bool> m_no_sleep{false};

    LockFreeRingBuffer<cv::Mat, 2> m_frame_buffer;
    cv::VideoCapture m_cap;
    std::atomic<bool> m_running{true};
    std::mutex m_mtx;
};
