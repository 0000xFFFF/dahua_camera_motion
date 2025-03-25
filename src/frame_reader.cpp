#include "debug.h"
#include "globals.hpp"
#include "utils.h"
#include <atomic>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

#include "frame_reader.hpp"

FrameReader::FrameReader(int ch, int w, int h, const std::string& ip,
                         const std::string& username, const std::string& password)
    :

      m_ip(ip),
      m_username(username),
      m_password(password),
      m_channel(ch),
      m_width(w),
      m_height(h)
{

    m_thread = std::thread([this]() { connect_and_read(); });
}

cv::Mat FrameReader::get_latest_frame()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_frame.empty()) return cv::Mat();
    return m_frame.clone();
}

void FrameReader::stop()
{
    D(std::cout << "[" << m_channel << "] reader stop" << std::endl);
    m_running = false;
    if (m_thread.joinable()) {
        D(std::cout << "[" << m_channel << "] reader join" << std::endl);
        m_thread.join();
        D(std::cout << "[" << m_channel << "] reader joined" << std::endl);
    }
    D(std::cout << "[" << m_channel << "] reader stopped" << std::endl);
}

std::string FrameReader::construct_rtsp_url(const std::string& ip, const std::string& username,
                                            const std::string& password)
{
    int subtype = (USE_SUBTYPE1 && m_channel != 0) ? 1 : 0;
    return "rtsp://" + username + ":" + password + "@" + ip +
           ":554/cam/realmonitor?channel=" + std::to_string(m_channel) +
           "&subtype=" + std::to_string(subtype);
}

void FrameReader::connect_and_read()
{

    set_thread_affinity(m_channel % std::thread::hardware_concurrency()); // Assign different core to each camera

    D(std::cout << "start capture: " << m_channel << std::endl);
    std::string rtsp_url = construct_rtsp_url(m_ip, m_username, m_password);

    // Set FFMPEG options for better stream handling
    m_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('H', '2', '6', '4'));
    m_cap.set(cv::CAP_PROP_BUFFERSIZE, 2);

    // use GPU
    m_cap.set(cv::CAP_PROP_HW_ACCELERATION, cv::VIDEO_ACCELERATION_ANY); // Use any available hardware acceleration
    m_cap.set(cv::CAP_PROP_HW_DEVICE, 0);                                // Use default GPU

    // Set environment variable for RTSP transport
    putenv((char*)"OPENCV_FFMPEG_CAPTURE_OPTIONS=rtsp_transport;udp");

    if (!m_cap.open(rtsp_url, cv::CAP_FFMPEG)) {
        throw std::runtime_error("Failed to open RTSP stream for channel " +
                                 std::to_string(m_channel));
    }

    D(std::cout << "connected: " << m_channel << std::endl);

    while (m_running) {
        if (!m_cap.isOpened()) {
            std::cerr << "Camera " << m_channel << " is not open. Exiting thread." << std::endl;
            break;
        }

        cv::Mat frame;
        if (!m_cap.read(frame)) { // Blocks until a frame is available
            std::cerr << "Failed to read frame from channel " << m_channel << std::endl;
#ifndef NODELAY
            std::this_thread::sleep_for(std::chrono::milliseconds(ERROR_SLEEP_MS)); // Prevent CPU overuse on failure
#endif
            continue;
        }

        std::lock_guard<std::mutex> lock(m_mtx);
        m_frame = std::move(frame);
    }

    if (m_cap.isOpened()) {
        m_cap.release();
    }

    D(std::cout << "Exiting readFrames() thread for channel " << m_channel << std::endl);
}
