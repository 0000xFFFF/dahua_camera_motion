#include "buffers.hpp"
#include <atomic>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

#ifdef SLEEP_MS_FRAME
#include <mutex>
#include <condition_variable>
#endif

class FrameReader {
  public:
    FrameReader(int channel,
                const std::string& ip,
                const std::string& username,
                const std::string& password);

    cv::Mat get_latest_frame(bool no_empty_frame);
#ifdef SLEEP_MS_FRAME
    void disable_sleep();
    void enable_sleep();
#endif
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
    std::atomic<double> captured_fps{15.0};

#ifdef SLEEP_MS_FRAME
    std::atomic<bool> m_sleep{true};
    std::mutex m_mtx;
    std::condition_variable m_cv;
#endif

    LockFreeRingBuffer<cv::Mat, 2> m_frame_buffer;
    DoubleBufferMat m_frame_dbuffer;
    cv::VideoCapture m_cap;
    std::atomic<bool> m_running{true};
};
