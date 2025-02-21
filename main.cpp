#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <deque>
#include <atomic>
#include <memory>
#include <vector>
#include <cstring>
#include <iostream>
#include <condition_variable>
#include <argparse/argparse.hpp>

// Constants
constexpr bool USE_SUBTYPE1 = false;
constexpr int W_0 = 704, H_0 = 576;
constexpr int W_HD = 1920, H_HD = 1080;

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
        
        std::string rtsp_url = constructRtspUrl(ip, username, password);
        
        // Set FFMPEG options
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('H', '2', '6', '4'));
        cap.set(cv::CAP_PROP_BUFFERSIZE, 2);
        
        if (!cap.open(rtsp_url, cv::CAP_FFMPEG)) {
            throw std::runtime_error("Failed to open RTSP stream for channel " + 
                                   std::to_string(channel));
        }
    }

    void readFrames() {
        running = true;
        cv::Mat frame;
        
        while (running) {
            if (cap.read(frame)) {
                std::lock_guard<std::mutex> lock(mtx);
                if (frame_queue.size() >= MAX_QUEUE_SIZE) {
                    frame_queue.pop_front();
                }
                frame_queue.push_back(frame.clone());
                cv.notify_one();
            }
        }
    }

    cv::Mat getLatestFrame() {
        std::unique_lock<std::mutex> lock(mtx);
        if (frame_queue.empty()) {
            cv.wait_for(lock, std::chrono::milliseconds(100));
        }
        return !frame_queue.empty() ? frame_queue.back().clone() : cv::Mat();
    }

    void stop() {
        running = false;
        cap.release();
    }

    ~FrameReader() {
        stop();
    }
};

class MotionDetector {
private:
    std::vector<std::unique_ptr<FrameReader>> readers;
    std::vector<std::thread> threads;
    cv::Ptr<cv::BackgroundSubtractorMOG2> fgbg;
    int current_channel;
    bool enableInfo;
    bool enableMotion;
    bool enableMinimap;
    std::atomic<bool> running{false};

    cv::Point getMinimapPosition(const cv::Size& frame_dims, 
                                const cv::Size& minimap_dims, int margin = 10) {
        return cv::Point(margin, margin);
    }

public:
    MotionDetector(const std::string& ip, const std::string& username, 
                   const std::string& password, int motion_area)
        : current_channel(1), enableInfo(true), enableMotion(true), enableMinimap(true) {
        
        // Initialize background subtractor
        fgbg = cv::createBackgroundSubtractorMOG2(500, 16, true);

        try {
            // Initialize channel 0 (motion detection camera)
            readers.push_back(std::make_unique<FrameReader>(0, W_0, H_0, ip, username, password));
            
            // Initialize channels 1-6 (HD cameras)
            for (int channel = 1; channel <= 6; ++channel) {
                readers.push_back(std::make_unique<FrameReader>(
                    channel, W_HD, H_HD, ip, username, password));
            }
        } catch (const std::exception& e) {
            std::cerr << "Error initializing cameras: " << e.what() << std::endl;
            throw;
        }
    }

    void start(int display_width, int display_height, bool fullscreen) {
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

        while (running) {
            // Get frame from motion detection camera
            cv::Mat frame0 = readers[0]->getLatestFrame();
            if (frame0.empty()) continue;

            // Crop frame0
            cv::Mat cropped = frame0(cv::Rect(0, 0, CROP_WIDTH, CROP_HEIGHT));
            cv::Rect motion_region;
            bool motion_detected = false;

            if (enableMotion) {
                cv::Mat fgmask;
                fgbg->apply(cropped, fgmask);
                
                cv::Mat thresh;
                cv::threshold(fgmask, thresh, 128, 255, cv::THRESH_BINARY);
                
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(thresh, contours, cv::RETR_EXTERNAL, 
                               cv::CHAIN_APPROX_SIMPLE);

                // Find largest motion area
                double max_area = 0;
                for (const auto& contour : contours) {
                    cv::Rect rect = cv::boundingRect(contour);
                    double area = rect.width * rect.height;
                    if (area > max_area) {
                        max_area = area;
                        motion_region = rect;
                        motion_detected = true;
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
            cv::Mat main_frame = readers[current_channel]->getLatestFrame();
            if (main_frame.empty()) continue;

            cv::Mat display;
            cv::resize(main_frame, display, cv::Size(display_width, display_height));

            // Process minimap
            if (enableMinimap) {
                cv::Mat minimap;
                cv::resize(cropped, minimap, cv::Size(MINIMAP_WIDTH, MINIMAP_HEIGHT));
                
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
                        static_cast<int>(motion_region.height * scale_y)
                    );
                    cv::rectangle(minimap_padded, scaled_rect, cv::Scalar(0, 255, 0), 1);
                }

                // Place minimap on main display
                cv::Point minimap_pos = getMinimapPosition(
                    cv::Size(display_width, display_height),
                    minimap_padded.size()
                );
                minimap_padded.copyTo(display(cv::Rect(
                    minimap_pos.x, minimap_pos.y,
                    minimap_padded.cols, minimap_padded.rows
                )));
            }

            // Draw info text
            if (enableInfo) {
                const int text_y_start = 200;
                const int text_y_step = 35;
                const cv::Scalar text_color(255, 255, 255);
                const double font_scale = 0.8;
                const int font_thickness = 2;

                cv::putText(display, "Info (i): " + std::string(enableInfo ? "Yes" : "No"),
                           cv::Point(10, text_y_start), cv::FONT_HERSHEY_SIMPLEX,
                           font_scale, text_color, font_thickness);
                cv::putText(display, "Motion (a): " + std::string(enableMotion ? "Yes" : "No"),
                           cv::Point(10, text_y_start + text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                           font_scale, text_color, font_thickness);
                cv::putText(display, "Minimap (m): " + std::string(enableMinimap ? "Yes" : "No"),
                           cv::Point(10, text_y_start + 2 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                           font_scale, text_color, font_thickness);
            }

            cv::imshow("Motion", display);

            char key = cv::waitKey(1);
            if (key == 'q') break;
            else if (key == 'a') enableMotion = !enableMotion;
            else if (key == 'i') enableInfo = !enableInfo;
            else if (key == 'm') enableMinimap = !enableMinimap;
            else if (key >= '1' && key <= '6') current_channel = key - '0';
        }
    }

    void stop() {
        running = false;
        for (auto& reader : readers) {
            reader->stop();
        }
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        cv::destroyAllWindows();
    }

    ~MotionDetector() {
        stop();
    }
};

int main(int argc, char* argv[]) {
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
        .default_value(10)
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

    try {
        program.parse_args(argc, argv);
        
        MotionDetector detector(
            program.get<std::string>("ip"),
            program.get<std::string>("username"),
            program.get<std::string>("password"),
            program.get<int>("area")
        );

        detector.start(
            program.get<int>("width"),
            program.get<int>("height"),
            program.get<bool>("fullscreen")
        );
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
