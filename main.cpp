#include <argparse/argparse.hpp>
#include <array>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <sched.h>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef DEBUG
#define ENABLE_INFO 1
#define ENABLE_MINIMAP 1
#define ENABLE_MOTION 0
#else
#define ENABLE_INFO 0
#define ENABLE_MINIMAP 0
#define ENABLE_MOTION 1
#endif

#define ENABLE_FULLSCREEN 0
#define MOTION_DETECT_AREA 10

#define DRAW_SLEEP_MS 100
#define READ_FRAME_SLEEP_MS 5
#define ERROR_SLEEP_MS 5

// Constants
constexpr bool USE_SUBTYPE1 = false;
constexpr int W_0 = 704, H_0 = 576;
constexpr int W_HD = 1920, H_HD = 1080;

void set_thread_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

// Helper function to execute shell commands
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Get screen resolution using xrandr
std::pair<int, int> detect_screen_size(const int& index) {
    try {
        std::string xrandr_output = exec("xrandr | grep '*' | awk '{print $1}'");
        std::vector<std::string> resolutions;
        std::stringstream ss(xrandr_output);
        std::string res;
        std::cout << "detected resolutions:" << std::endl;
        while (std::getline(ss, res, '\n')) {
            std::cout << res << std::endl;
            resolutions.push_back(res);
        }
        const std::string& use = resolutions[index];
        std::cout << "using: " << use << std::endl;

        size_t x_pos = use.find('x');
        if (x_pos != std::string::npos) {
            int width = std::stoi(use.substr(0, x_pos));
            int height = std::stoi(use.substr(x_pos + 1));
            return {width, height};
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to detect screen size: " << e.what() << std::endl;
    }
    return {W_HD, H_HD}; // Default fallback
}

class FrameReader {

  public:
    FrameReader(int ch, int w, int h, const std::string& ip,
                const std::string& username, const std::string& password)
        :

          m_ip(ip),
          m_username(username),
          m_password(password),
          m_channel(ch),
          m_width(w),
          m_height(h) {

        m_thread = std::thread([this]() { connect_and_read(); });
    }

    cv::Mat get_latest_frame() {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (m_frame_queue.empty()) return cv::Mat();

        cv::Mat latest = std::move(m_frame_queue.back()); // Avoid deep copy
        m_frame_queue.clear();                            // Drop all old frames
        return latest;
    }

    void stop() {
        std::cout << "[" << m_channel << "] reader stop" << std::endl;
        m_running = false;
        if (m_thread.joinable()) {
            std::cout << "[" << m_channel << "] reader join" << std::endl;
            m_thread.join();
            std::cout << "[" << m_channel << "] reader joined" << std::endl;
        }
        std::cout << "[" << m_channel << "] reader stopped" << std::endl;
    }

  private:
    std::thread m_thread;
    std::string m_ip;
    std::string m_username;
    std::string m_password;
    int m_channel;
    int m_width;
    int m_height;
    std::deque<cv::Mat> m_frame_queue;
    cv::VideoCapture m_cap;
    std::atomic<bool> m_running{true};
    std::mutex m_mtx;
    static constexpr size_t MAX_QUEUE_SIZE = 2;

    std::string construct_rtsp_url(const std::string& ip, const std::string& username,
                                   const std::string& password) {
        int subtype = (USE_SUBTYPE1 && m_channel != 0) ? 1 : 0;
        return "rtsp://" + username + ":" + password + "@" + ip +
               ":554/cam/realmonitor?channel=" + std::to_string(m_channel) +
               "&subtype=" + std::to_string(subtype);
    }

    void connect_and_read() {

        set_thread_affinity(m_channel % std::thread::hardware_concurrency()); // Assign different core to each camera

        std::cout << "start capture: " << m_channel << std::endl;
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

        std::cout << "connected: " << m_channel << std::endl;

        cv::Mat frame;

        while (m_running) {
            if (!m_cap.isOpened()) {
                std::cerr << "Camera " << m_channel << " is not open. Exiting thread." << std::endl;
                break;
            }

            // Use grab + retrieve instead of read() to prevent blocking
            if (m_cap.grab()) {        // Grabs the frame (non-blocking)
                m_cap.retrieve(frame); // Retrieves the frame (blocking only if available)

                std::lock_guard<std::mutex> lock(m_mtx);
                if (m_frame_queue.size() >= MAX_QUEUE_SIZE) {
                    m_frame_queue.pop_front();
                }
                m_frame_queue.emplace_back(std::move(frame));
            } else {
                std::cerr << "Failed to grab frame from channel " << m_channel << std::endl;

#ifndef NODELAY
                std::this_thread::sleep_for(std::chrono::milliseconds(ERROR_SLEEP_MS)); // Prevent CPU overuse
#endif
            }
        }

        if (m_cap.isOpened()) {
            m_cap.release();
        }

        std::cout << "Exiting readFrames() thread for channel " << m_channel << std::endl;
    }
};

class MotionDetector {

  public:
    MotionDetector(const std::string& ip, const std::string& username,
                   const std::string& password, int area, int w, int h, bool fullscreen)
        : m_current_channel(1),
          m_enableInfo(ENABLE_INFO),
          m_enableMotion(ENABLE_MOTION),
          m_enableMinimap(ENABLE_MINIMAP),
          m_enableFullscreen(ENABLE_FULLSCREEN),
          m_motion_area(area),
          m_display_width(w),
          m_display_height(h),
          m_fullscreen(fullscreen) {

        // Initialize background subtractor
        m_fgbg = cv::createBackgroundSubtractorMOG2(500, 16, true);

        m_readers.push_back(std::make_unique<FrameReader>(0, W_0, H_0, ip, username, password));

        for (int channel = 1; channel <= 6; ++channel) {
            m_readers.push_back(std::make_unique<FrameReader>(
                channel, W_HD, H_HD, ip, username, password));
        }
    }

    cv::Mat paint_main_mat(cv::Mat& main_mat) {
        // Assume readers[i] are objects that can get frames
        for (int i = 0; i < 6; i++) {
            cv::Mat mat = m_readers[i + 1]->get_latest_frame();

            if (mat.empty()) {
                continue; // If the frame is empty, skip painting
            }

            // Compute position in the 3x2 grid
            int row = i / 3; // 0 for first row, 1 for second row
            int col = i % 3; // Column position (0,1,2)

            // Define the region of interest (ROI) in main_mat
            cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);

            // Copy the frame into main_mat at the correct position
            mat.copyTo(main_mat(roi));
        }

        return main_mat;
    }

    void start() {
        m_running = true;

        if (m_fullscreen) {
            cv::namedWindow("Motion", cv::WINDOW_NORMAL);
            cv::setWindowProperty("Motion", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
        }

        constexpr int MINIMAP_WIDTH = 300;
        constexpr int MINIMAP_HEIGHT = 160;
        constexpr int CROP_HEIGHT = 384;
        constexpr int CROP_WIDTH = 704;

        cv::Mat frame0 = cv::Mat::zeros(W_0, H_0, CV_8UC3);
        cv::Mat main_mat(cv::Size(W_HD * 3, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0));
        cv::Mat main_frame = cv::Mat::zeros(W_HD, H_HD, CV_8UC3);

        try {

            while (m_running) {
#ifndef NODELAY
                std::this_thread::sleep_for(std::chrono::milliseconds(DRAW_SLEEP_MS)); // Prevent CPU overuse
#endif


                cv::Mat frame0_get = m_readers[0]->get_latest_frame();
                if (!frame0_get.empty()) {
                    frame0 = frame0_get(cv::Rect(0, 0, CROP_WIDTH, CROP_HEIGHT));
                }

                cv::Rect motion_region;
                bool motion_detected = false;

                if (m_enableMotion) {
                    cv::Mat fgmask;
                    m_fgbg->apply(frame0, fgmask);

                    cv::Mat thresh;
                    cv::threshold(fgmask, thresh, 128, 255, cv::THRESH_BINARY);

                    std::vector<std::vector<cv::Point>> contours;
                    cv::findContours(thresh, contours, cv::RETR_EXTERNAL,
                                     cv::CHAIN_APPROX_SIMPLE);

                    // Find largest motion area
                    double max_area = 0;
                    for (const auto& contour : contours) {
                        if (cv::contourArea(contour) >= m_motion_area) {
                            cv::Rect rect = cv::boundingRect(contour);
                            double area = rect.width * rect.height;
                            if (area > max_area) {
                                max_area = area;
                                motion_region = rect;
                                motion_detected = true;
                            }
                        }
                    }

                    // Update current channel based on motion position
                    if (motion_detected) {
                        float rel_x = motion_region.x / static_cast<float>(CROP_WIDTH);
                        float rel_y = motion_region.y / static_cast<float>(CROP_HEIGHT);
                        int new_channel = 1 + static_cast<int>(rel_x * 3) +
                                          (rel_y >= 0.5f ? 3 : 0);
                        if (new_channel != m_current_channel) {
                            m_current_channel = new_channel;
                        }
                    }
                }

                // Get main display frame
                cv::Mat main_frame_get = (m_enableFullscreen || motion_detected) ? m_readers[m_current_channel]->get_latest_frame() : paint_main_mat(main_mat);
                if (!main_frame_get.empty()) {
                    cv::resize(main_frame_get, main_frame, cv::Size(m_display_width, m_display_height));
                }

                // Process minimap
                if (m_enableMinimap) {
                    cv::Mat minimap;
                    cv::resize(frame0, minimap, cv::Size(MINIMAP_WIDTH, MINIMAP_HEIGHT));

                    // Add white border
                    cv::Mat minimap_padded;
                    cv::copyMakeBorder(minimap, minimap_padded, 2, 2, 2, 2,
                                       cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

                    // Draw motion rectangle on minimap
                    if (motion_detected) {
                        float scale_x = static_cast<float>(MINIMAP_WIDTH) / CROP_WIDTH;
                        float scale_y = static_cast<float>(MINIMAP_HEIGHT) / CROP_HEIGHT;
                        cv::Rect scaled_rect(
                            static_cast<int>(motion_region.x * scale_x),
                            static_cast<int>(motion_region.y * scale_y),
                            static_cast<int>(motion_region.width * scale_x),
                            static_cast<int>(motion_region.height * scale_y));
                        cv::rectangle(minimap_padded, scaled_rect, cv::Scalar(0, 255, 0), 1);
                    }

                    // Place minimap on main display
                    cv::Point minimap_pos = cv::Point(10, 10);
                    minimap_padded.copyTo(main_frame(cv::Rect(
                        minimap_pos.x, minimap_pos.y,
                        minimap_padded.cols, minimap_padded.rows)));
                }

                // Draw info text
                if (m_enableInfo) {
                    const int text_y_start = 200;
                    const int text_y_step = 35;
                    const cv::Scalar text_color(255, 255, 255);
                    const double font_scale = 0.8;
                    const int font_thickness = 2;

                    cv::putText(main_frame, "Info (i): " + bool_to_str(m_enableInfo),
                                cv::Point(10, text_y_start), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Motion (a): " + bool_to_str(m_enableMotion),
                                cv::Point(10, text_y_start + text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Minimap (m): " + bool_to_str(m_enableMinimap),
                                cv::Point(10, text_y_start + 2 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Motion Detected: " + bool_to_str(motion_detected),
                                cv::Point(10, text_y_start + 3 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Channel (num): " + std::to_string(m_current_channel),
                                cv::Point(10, text_y_start + 4 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Fullscreen (f): " + bool_to_str(m_enableFullscreen),
                                cv::Point(10, text_y_start + 5 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                }

                cv::imshow("Motion", main_frame);

                char key = cv::waitKey(1);
                if (key == 'q') {
                    stop();
                    break;
                } else if (key == 'a') {
                    m_enableMotion = !m_enableMotion;
                } else if (key == 'i') {
                    m_enableInfo = !m_enableInfo;
                } else if (key == 'm') {
                    m_enableMinimap = !m_enableMinimap;
                } else if (key == 'f') {
                    m_enableFullscreen = !m_enableFullscreen;
                } else if (key >= '1' && key <= '6') {
                    m_current_channel = key - '0';
                    m_enableFullscreen = true;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in display loop: " << e.what() << std::endl;
        }
    }

    void stop() {
        m_running = false;

        std::cout << "stopping all readers" << std::endl;
        for (auto& reader : m_readers) {
            reader->stop();
        }
        std::cout << "stopped all readers" << std::endl;

        std::cout << "close all wins" << std::endl;
        cv::destroyAllWindows();
        std::cout << "closed all wins" << std::endl;
    }

  private:
    std::vector<std::unique_ptr<FrameReader>> m_readers;
    cv::Ptr<cv::BackgroundSubtractorMOG2> m_fgbg;
    int m_current_channel;
    bool m_enableInfo;
    bool m_enableMotion;
    bool m_enableMinimap;
    bool m_enableFullscreen;
    int m_motion_area;
    int m_display_width;
    int m_display_height;
    bool m_fullscreen;
    std::atomic<bool> m_running{false};

    std::string bool_to_str(bool b) {
        return std::string(b ? "Yes" : "No");
    }
};

int main(int argc, char* argv[]) {

    // Add signal handling
    std::signal(SIGINT, [](int) {
        cv::destroyAllWindows();
        std::exit(0);
    });

    argparse::ArgumentParser program("dcm_master");

    program.add_argument("-i", "--ip")
        .help("IP to connect to")
        .required();
    program.add_argument("-u", "--username")
        .help("Account username")
        .required();
    program.add_argument("-p", "--password")
        .help("Account password")
        .required();
    program.add_argument("-a", "--area")
        .help("Contour area for detection")
        .default_value(MOTION_DETECT_AREA)
        .scan<'i', int>();
    program.add_argument("-ww", "--width")
        .help("Window width")
        .default_value(1920)
        .scan<'i', int>();
    program.add_argument("-wh", "--height")
        .help("Window height")
        .default_value(1080)
        .scan<'i', int>();
    program.add_argument("-fs", "--fullscreen")
        .help("Start in fullscreen mode")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-d", "--detect")
        .help("Detect screen size with xrandr")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-r", "--resolution")
        .help("index of resolution to use (default: 0)")
        .default_value(0)
        .scan<'i', int>();

    try {
        program.parse_args(argc, argv);

        int width = program.get<int>("width");
        int height = program.get<int>("height");

        // Override width/height with detected screen size if requested
        if (program.get<bool>("detect")) {
            auto [detected_width, detected_height] = detect_screen_size(program.get<int>("resolution"));
            width = detected_width;
            height = detected_height;
            std::cout << "Detected screen size: " << width << "x" << height << std::endl;
        }

        {
            MotionDetector motionDetector(
                program.get<std::string>("ip"),
                program.get<std::string>("username"),
                program.get<std::string>("password"),
                program.get<int>("area"), width, height, program.get<bool>("fullscreen"));

            motionDetector.start();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
