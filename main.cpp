#include <argparse/argparse.hpp>
#include <array>
#include <atomic>
#include <condition_variable>
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
#else
#define ENABLE_INFO 0
#define ENABLE_MINIMAP 0
#endif

#define ENABLE_MOTION 1
#define ENABLE_FULLSCREEN 0
#define MOTION_DETECT_AREA 10

#define DRAW_SLEEP_MS 10
#define READ_FRAME_SLEEP_MS 5
#define ERROR_SLEEP_MS 5

// Constants
constexpr bool USE_SUBTYPE1 = false;
constexpr int W_0 = 704, H_0 = 576;
constexpr int W_HD = 1920, H_HD = 1080;

void setThreadAffinity(int core_id) {
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
    return {1920, 1080}; // Default fallback
}

class FrameReader {
  private:
    int channel;
    int width;
    int height;
    std::deque<cv::Mat> frame_queue;
    cv::VideoCapture cap;
    std::atomic<bool> running{false};
    std::mutex mtx;
    std::condition_variable cv;
    static constexpr size_t MAX_QUEUE_SIZE = 2;

    std::string constructRtspUrl(const std::string& ip, const std::string& username,
                                 const std::string& password) {
        int subtype = (USE_SUBTYPE1 && channel != 0) ? 1 : 0;
        return "rtsp://" + username + ":" + password + "@" + ip +
               ":554/cam/realmonitor?channel=" + std::to_string(channel) +
               "&subtype=" + std::to_string(subtype);
    }

  public:
    FrameReader(int ch, int w, int h, const std::string& ip,
                const std::string& username, const std::string& password)
        : channel(ch), width(w), height(h) {

        std::cout << "start capture: " << ch << std::endl;
        std::string rtsp_url = constructRtspUrl(ip, username, password);

        // Set FFMPEG options for better stream handling
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('H', '2', '6', '4'));
        cap.set(cv::CAP_PROP_BUFFERSIZE, 2);

        // use GPU
        cap.set(cv::CAP_PROP_HW_ACCELERATION, cv::VIDEO_ACCELERATION_ANY); // Use any available hardware acceleration
        cap.set(cv::CAP_PROP_HW_DEVICE, 0);                                // Use default GPU

        // Set environment variable for RTSP transport
        putenv((char*)"OPENCV_FFMPEG_CAPTURE_OPTIONS=rtsp_transport;udp");

        if (!cap.open(rtsp_url, cv::CAP_FFMPEG)) {
            throw std::runtime_error("Failed to open RTSP stream for channel " +
                                     std::to_string(channel));
        }
    }

    void readFrames() {
        running = true;
        cv::Mat frame;

        setThreadAffinity(channel % std::thread::hardware_concurrency()); // Assign different core to each camera

        while (running) {
            if (!cap.isOpened()) {
                std::cerr << "Camera " << channel << " is not open. Exiting thread." << std::endl;
                break;
            }

            // Use grab + retrieve instead of read() to prevent blocking
            if (cap.grab()) {        // Grabs the frame (non-blocking)
                cap.retrieve(frame); // Retrieves the frame (blocking only if available)

                std::lock_guard<std::mutex> lock(mtx);
                if (frame_queue.size() >= MAX_QUEUE_SIZE) {
                    frame_queue.pop_front();
                }
                frame_queue.emplace_back(std::move(frame));
                cv.notify_one();
            } else {
                std::cerr << "Failed to grab frame from channel " << channel << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(ERROR_SLEEP_MS)); // Prevent CPU overuse
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(READ_FRAME_SLEEP_MS)); // Prevent CPU overuse
        }

        if (cap.isOpened()) {
            cap.release();
        }

        // std::cout << "Exiting readFrames() thread for channel " << channel << std::endl;
    }

    cv::Mat getLatestFrame() {
        std::unique_lock<std::mutex> lock(mtx);
        if (frame_queue.empty()) return cv::Mat();

        cv::Mat latest = std::move(frame_queue.back()); // Avoid deep copy
        frame_queue.clear();                            // Drop all old frames
        return latest;
    }

    void stop() {
        running = false; // Stop the loop

        // Wake up waiting threads so they do not block
        cv.notify_all();
    }

    ~FrameReader() {
        try {
            stop();
        } catch (...) {
            std::cerr << "Error during FrameReader destruction" << std::endl;
        }
    }
};

class MotionDetector {
  private:
    std::vector<std::unique_ptr<FrameReader>> readers;
    std::vector<std::thread> init_threads; // New member for initialization threads
    std::vector<std::thread> threads;
    cv::Ptr<cv::BackgroundSubtractorMOG2> fgbg;
    int current_channel;
    bool enableInfo;
    bool enableMotion;
    bool enableMinimap;
    bool enableFullscreen;
    int motion_area;
    int display_width;
    int display_height;
    std::atomic<bool> running{false};

    cv::Point getMinimapPosition(const cv::Size& frame_dims,
                                 const cv::Size& minimap_dims, int margin = 10) {
        return cv::Point(margin, margin);
    }

    std::string bool_to_str(bool b) {
        return std::string(b ? "Yes" : "No");
    }

  public:
    MotionDetector(const std::string& ip, const std::string& username,
                   const std::string& password, int area, int w, int h)
        : current_channel(1),
          enableInfo(ENABLE_INFO),
          enableMotion(ENABLE_MOTION),
          enableMinimap(ENABLE_MINIMAP),
          enableFullscreen(ENABLE_FULLSCREEN),
          motion_area(area),
          display_width(w),
          display_height(h) {

        // Initialize background subtractor
        fgbg = cv::createBackgroundSubtractorMOG2(500, 16, true);

        readers.resize(7); // For channels 0-6

        // Start threads to initialize each camera connection in parallel
        for (int channel = 0; channel <= 6; ++channel) {
            init_threads.emplace_back([this, channel, ip, username, password]() {
                try {
                    int w = (channel == 0) ? W_0 : W_HD;
                    int h = (channel == 0) ? H_0 : H_HD;
                    readers[channel] = std::make_unique<FrameReader>(
                        channel, w, h, ip, username, password);
                } catch (const std::exception& e) {
                    std::cerr << "Error initializing camera " << channel
                              << ": " << e.what() << std::endl;
                }
            });
        }

        // Wait for all initialization threads to complete
        for (auto& thread : init_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // Check if all cameras were initialized successfully
        for (int i = 0; i <= 6; ++i) {
            if (!readers[i]) {
                throw std::runtime_error("Failed to initialize camera " + std::to_string(i));
            }
        }
    }

    cv::Mat paint_main_mat(cv::Mat& main_mat) {
        // Assume readers[i] are objects that can get frames
        for (int i = 0; i < 6; i++) {
            cv::Mat mat = readers[i + 1]->getLatestFrame();

            if (mat.empty()) {
                continue; // If the frame is empty, skip painting
            }

            // Compute position in the 3x2 grid
            int row = i / 3; // 0 for first row, 1 for second row
            int col = i % 3; // Column position (0,1,2)

            // Define the region of interest (ROI) in main_mat
            cv::Rect roi(col * display_width, row * display_height, display_width, display_height);

            // Copy the frame into main_mat at the correct position
            mat.copyTo(main_mat(roi));
        }

        return main_mat;
    }

    void start(bool fullscreen) {
        running = true;

        // Start frame reading threads
        for (auto& reader : readers) {
            threads.emplace_back(&FrameReader::readFrames, reader.get());
        }

        if (fullscreen) {
            cv::namedWindow("Motion", cv::WINDOW_NORMAL);
            cv::setWindowProperty("Motion", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
        }

        constexpr int MINIMAP_WIDTH = 300;
        constexpr int MINIMAP_HEIGHT = 160;
        constexpr int CROP_HEIGHT = 384;
        constexpr int CROP_WIDTH = 704;

        cv::Mat main_mat(cv::Size(display_width * 3, display_height * 2), CV_8UC3, cv::Scalar(0, 0, 0));
        cv::Mat main_frame = cv::Mat::zeros(display_width, display_height, CV_8UC3);

        std::cout << "DRAW LOOP" << std::endl;

        try {

            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(DRAW_SLEEP_MS)); // Prevent CPU overuse

                cv::Mat frame0 = readers[0]->getLatestFrame();
                if (frame0.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(DRAW_SLEEP_MS)); // Prevent CPU overuse
                    continue;
                }

                // crop frame0
                frame0 = frame0(cv::Rect(0, 0, CROP_WIDTH, CROP_HEIGHT));


                // Crop frame0
                cv::Rect motion_region;
                bool motion_detected = false;

                if (enableMotion) {
                    cv::Mat fgmask;
                    fgbg->apply(frame0, fgmask);

                    cv::Mat thresh;
                    cv::threshold(fgmask, thresh, 128, 255, cv::THRESH_BINARY);

                    std::vector<std::vector<cv::Point>> contours;
                    cv::findContours(thresh, contours, cv::RETR_EXTERNAL,
                                     cv::CHAIN_APPROX_SIMPLE);

                    // Find largest motion area
                    double max_area = 0;
                    for (const auto& contour : contours) {
                        if (cv::contourArea(contour) >= motion_area) {
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
                        if (new_channel != current_channel) {
                            current_channel = new_channel;
                        }
                    }
                }

                // Get main display frame
                cv::Mat main_frame_get = (enableFullscreen || motion_detected) ? readers[current_channel]->getLatestFrame() : paint_main_mat(main_mat);
                if (!main_frame_get.empty()) {
                    cv::resize(main_frame_get, main_frame, cv::Size(display_width, display_height));
                }

                // Process minimap
                if (enableMinimap) {
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
                    cv::Point minimap_pos = getMinimapPosition(
                        cv::Size(display_width, display_height),
                        minimap_padded.size());
                    minimap_padded.copyTo(main_frame(cv::Rect(
                        minimap_pos.x, minimap_pos.y,
                        minimap_padded.cols, minimap_padded.rows)));
                }

                // Draw info text
                if (enableInfo) {
                    const int text_y_start = 200;
                    const int text_y_step = 35;
                    const cv::Scalar text_color(255, 255, 255);
                    const double font_scale = 0.8;
                    const int font_thickness = 2;

                    cv::putText(main_frame, "Info (i): " + bool_to_str(enableInfo),
                                cv::Point(10, text_y_start), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Motion (a): " + bool_to_str(enableMotion),
                                cv::Point(10, text_y_start + text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Minimap (m): " + bool_to_str(enableMinimap),
                                cv::Point(10, text_y_start + 2 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Motion Detected: " + bool_to_str(motion_detected),
                                cv::Point(10, text_y_start + 3 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Channel (num): " + std::to_string(current_channel),
                                cv::Point(10, text_y_start + 4 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                    cv::putText(main_frame, "Fullscreen (f): " + bool_to_str(enableFullscreen),
                                cv::Point(10, text_y_start + 5 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                                font_scale, text_color, font_thickness);
                }

                cv::imshow("Motion", main_frame);

                char key = cv::waitKey(1);
                if (key == 'q') {
                    running = false;
                    break;
                } else if (key == 'a') {
                    enableMotion = !enableMotion;
                } else if (key == 'i') {
                    enableInfo = !enableInfo;
                } else if (key == 'm') {
                    enableMinimap = !enableMinimap;
                } else if (key == 'f') {
                    enableFullscreen = !enableFullscreen;
                } else if (key >= '1' && key <= '6') {
                    current_channel = key - '0';
                    enableFullscreen = true;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in display loop: " << e.what() << std::endl;
        }

        stop();
    }

    void stop() {
        running = false;

        for (auto& reader : readers) {
            // std::cout << "stop reader" << std::endl;
            reader->stop();
        }

        for (auto& thread : threads) {
            if (thread.joinable()) {
                // std::cout << "th join" << std::endl;
                thread.join();
            }
        }

        // std::cout << "close all wins" << std::endl;
        cv::destroyAllWindows();
    }

    ~MotionDetector() {
        try {
            stop();
        } catch (...) {
            // Prevent exceptions from escaping destructor
            std::cerr << "Error during MotionDetector cleanup" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {

    // Add signal handling
    std::signal(SIGINT, [](int) {
        cv::destroyAllWindows();
        std::exit(0);
    });

    argparse::ArgumentParser program("motion_detector");

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

        MotionDetector detector(
            program.get<std::string>("ip"),
            program.get<std::string>("username"),
            program.get<std::string>("password"),
            program.get<int>("area"), width, height);

        detector.start(program.get<bool>("fullscreen"));
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
