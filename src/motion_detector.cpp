#include "motion_detector.hpp"
#include "globals.hpp"
#include <opencv2/opencv.hpp>
#include <string>

#define MINIMAP_WIDTH 300
#define MINIMAP_HEIGHT 160
#define CROP_HEIGHT 384
#define CROP_WIDTH 704

MotionDetector::MotionDetector(const std::string& ip, const std::string& username,
                               const std::string& password, int area, int w, int h, bool fullscreen)
    : m_motion_area(area),
      m_display_width(w),
      m_display_height(h),
      m_fullscreen(fullscreen)
{

    // Initialize background subtractor
    m_fgbg = cv::createBackgroundSubtractorMOG2(500, 16, true);

    m_readers.push_back(std::make_unique<FrameReader>(0, W_0, H_0, ip, username, password));

    for (int channel = 1; channel <= 6; ++channel) {
        m_readers.push_back(std::make_unique<FrameReader>(
            channel, W_HD, H_HD, ip, username, password));
    }
}

void MotionDetector::do_tour_logic()
{
    m_tour_frame_index++;
    if (m_tour_frame_index >= m_tour_frame_count) {
        m_tour_frame_index = 0;
        m_current_channel = m_current_channel % 6 + 1;
    }
}

std::vector<std::vector<cv::Point>> MotionDetector::find_contours(cv::Mat& frame0)
{
    cv::Mat fgmask;
    m_fgbg->apply(frame0, fgmask);

    cv::Mat thresh;
    cv::threshold(fgmask, thresh, 128, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    return contours;
}

void MotionDetector::detect_largest_motion_set_channel(cv::Mat& frame0)
{
    std::vector<std::vector<cv::Point>> contours = find_contours(frame0);

    // Find largest motion area
    double max_area = 0;
    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= m_motion_area) {
            cv::Rect rect = cv::boundingRect(contour);
            double area = rect.width * rect.height;
            if (area > max_area) {
                max_area = area;
                m_motion_region = rect;
                m_motion_detected = true;
            }
        }
    }

    // Update current channel based on motion position
    if (m_motion_detected) {
        m_motion_detected_frame_count++;
        m_motion_detected_min_frames = m_motion_detected_frame_count >= MOTION_DETECT_MIN_FRAMES;

        if (m_motion_detected_min_frames) {
            m_tour_frame_index = 0; // reset so it doesn't auto switch on new tour so we can show a little bit of motion
            float rel_x = m_motion_region.x / static_cast<float>(CROP_WIDTH);
            float rel_y = m_motion_region.y / static_cast<float>(CROP_HEIGHT);
            int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);
            if (new_channel != m_current_channel) { m_current_channel = new_channel; }
        }
    }
    else {
        if (m_motion_detected_frame_count != 0) {
            m_motion_detected_frame_count_history = m_motion_detected_frame_count;
        }
        m_motion_detected_frame_count = 0;
    }
}

void MotionDetector::detect_all_motion_set_channels(cv::Mat& frame0)
{
    std::vector<std::vector<cv::Point>> contours = find_contours(frame0);

    m_sorted_chs_area = {
        std::make_pair(1, 0.0),
        std::make_pair(2, 0.0),
        std::make_pair(3, 0.0),
        std::make_pair(4, 0.0),
        std::make_pair(5, 0.0),
        std::make_pair(6, 0.0)};

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= m_motion_area) {
            cv::Rect rect = cv::boundingRect(contour);
            double area = rect.width * rect.height;

            float rel_x = rect.x / static_cast<float>(CROP_WIDTH);
            float rel_y = rect.y / static_cast<float>(CROP_HEIGHT);
            int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);

            if (new_channel >= 1 && new_channel <= 6) {
                m_sorted_chs_area[new_channel - 1].second = area;
            }
        }
    }

    // higher channel first
    std::sort(m_sorted_chs_area.begin(), m_sorted_chs_area.end(), [](auto& a, auto& b) { return a.second > b.second; });
}

void MotionDetector::draw_minimap(cv::Mat& frame0, cv::Mat& main_frame)
{
    cv::Mat minimap;
    cv::resize(frame0, minimap, cv::Size(MINIMAP_WIDTH, MINIMAP_HEIGHT));

    // Add white border
    cv::Mat minimap_padded;
    cv::copyMakeBorder(minimap, minimap_padded, 2, 2, 2, 2,
                       cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

    // Draw motion rectangle on minimap
    if (m_motion_detected) {
        float scale_x = static_cast<float>(MINIMAP_WIDTH) / CROP_WIDTH;
        float scale_y = static_cast<float>(MINIMAP_HEIGHT) / CROP_HEIGHT;
        cv::Rect scaled_rect(
            static_cast<int>(m_motion_region.x * scale_x),
            static_cast<int>(m_motion_region.y * scale_y),
            static_cast<int>(m_motion_region.width * scale_x),
            static_cast<int>(m_motion_region.height * scale_y));
        cv::rectangle(minimap_padded, scaled_rect, cv::Scalar(0, 255, 0), 1);
    }

    // Place minimap on main display
    cv::Point minimap_pos = cv::Point(10, 10);
    minimap_padded.copyTo(main_frame(cv::Rect(
        minimap_pos.x, minimap_pos.y,
        minimap_padded.cols, minimap_padded.rows)));
}

cv::Mat MotionDetector::paint_main_mat_all(cv::Mat& main_mat)
{
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

cv::Mat MotionDetector::paint_main_mat_some(cv::Mat& main_mat)
{
    // Assume readers[i] are objects that can get frames
    for (size_t i = 0; i < m_sorted_chs_area.size(); i++) {
        cv::Mat mat = m_readers[m_sorted_chs_area[i].first]->get_latest_frame();

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

void MotionDetector::start()
{
    m_running = true;

    if (m_fullscreen) {
        cv::namedWindow("Motion", cv::WINDOW_NORMAL);
        cv::setWindowProperty("Motion", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
    }

    cv::Mat frame0 = cv::Mat::zeros(W_0, H_0, CV_8UC3);
    cv::Mat main_mat(cv::Size(W_HD * 3, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat main_frame = cv::Mat::zeros(W_HD, H_HD, CV_8UC3);

    try {

        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(DRAW_SLEEP_MS)); // Prevent CPU overuse

            cv::Mat frame0_get = m_readers[0]->get_latest_frame();
            if (!frame0_get.empty()) {
                frame0 = frame0_get(cv::Rect(0, 0, CROP_WIDTH, CROP_HEIGHT));
            }

            cv::Mat main_frame_get = cv::Mat();

            if (m_enableTour) { do_tour_logic(); }
            if (m_enableMotion && m_enableMotionLargest) { detect_largest_motion_set_channel(frame0); }
            if (m_enableMotion && m_enableMotionAll) { detect_all_motion_set_channels(frame0); }

            // Get main display frame
            if (m_enableFullscreen || m_enableTour || m_motion_detected_min_frames) {
                main_frame_get = m_readers[m_current_channel]->get_latest_frame();
            }
            else if (m_enableMotionAll) {
                main_frame_get = paint_main_mat_some(main_mat);
            }
            else {
                main_frame_get = paint_main_mat_all(main_mat);
            }

            // check if null and resize
            if (!main_frame_get.empty()) { cv::resize(main_frame_get, main_frame, cv::Size(m_display_width, m_display_height)); }

            // Process minimap
            if (m_enableMinimap) { draw_minimap(frame0, main_frame); }

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
                cv::putText(main_frame, "Motion (m/l/a): " + bool_to_str(m_enableMotion) + "/" + bool_to_str(m_enableMotionLargest) + "/" + bool_to_str(m_enableMotionAll),
                            cv::Point(10, text_y_start + text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                            font_scale, text_color, font_thickness);
                cv::putText(main_frame, "Minimap (o): " + bool_to_str(m_enableMinimap),
                            cv::Point(10, text_y_start + 2 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                            font_scale, text_color, font_thickness);
                cv::putText(main_frame, "Motion Detected: " + std::to_string(m_motion_detected) + "/" + std::to_string(m_motion_detected_min_frames) + "/" + std::to_string(m_motion_detected_frame_count) + "/" + std::to_string(m_motion_detected_frame_count_history),
                            cv::Point(10, text_y_start + 3 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                            font_scale, text_color, font_thickness);
                cv::putText(main_frame, "Channel (num): " + std::to_string(m_current_channel),
                            cv::Point(10, text_y_start + 4 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                            font_scale, text_color, font_thickness);
                cv::putText(main_frame, "Fullscreen (f): " + bool_to_str(m_enableFullscreen),
                            cv::Point(10, text_y_start + 5 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                            font_scale, text_color, font_thickness);
                cv::putText(main_frame, "Tour (t): " + bool_to_str(m_enableTour) + " " + std::to_string(m_tour_frame_index) + "/" + std::to_string(m_tour_frame_count),
                            cv::Point(10, text_y_start + 6 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                            font_scale, text_color, font_thickness);
                cv::putText(main_frame, "Reset (r)",
                            cv::Point(10, text_y_start + 7 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                            font_scale, text_color, font_thickness);
            }

            cv::imshow("Motion", main_frame);

            char key = cv::waitKey(1);
            if (key == 'q') {
                stop();
                break;
            }
            else if (key == 'm') {
                m_enableMotion = !m_enableMotion;
            }
            else if (key == 'l') {
                m_enableMotionLargest = !m_enableMotionLargest;
            }
            else if (key == 'a') {
                m_enableMotionAll = !m_enableMotionAll;
            }
            else if (key == 'i') {
                m_enableInfo = !m_enableInfo;
            }
            else if (key == 'o') {
                m_enableMinimap = !m_enableMinimap;
            }
            else if (key == 'f') {
                m_enableFullscreen = !m_enableFullscreen;
            }
            else if (key == 'r') {
                m_enableInfo = ENABLE_INFO;
                m_enableMotion = ENABLE_MOTION;
                m_enableMinimap = ENABLE_MINIMAP;
                m_enableFullscreen = ENABLE_FULLSCREEN;
                m_enableTour = ENABLE_TOUR;
            }
            else if (key == 't') {
                m_enableTour = !m_enableTour;
            }
            else if (key >= '1' && key <= '6') {
                m_current_channel = key - '0';
                m_enableFullscreen = true;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in display loop: " << e.what() << std::endl;
    }
}

void MotionDetector::stop()
{
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

std::string MotionDetector::bool_to_str(bool b)
{
    return std::string(b ? "Yes" : "No");
}
