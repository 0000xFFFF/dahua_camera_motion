#include <atomic>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

class FrameReader {
  public:
    FrameReader(int ch, int w, int h, const std::string& ip,
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
    int m_width;
    int m_height;
    cv::Mat m_frame;
    cv::VideoCapture m_cap;
    std::atomic<bool> m_running{true};
    std::mutex m_mtx;
};
