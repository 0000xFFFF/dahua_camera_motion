#include "motion_detector.hpp"
#include "debug.h"
#include "globals.hpp"
#include "opencv2/core.hpp"
#include <exception>
#include <iostream>
#include <opencv2/bgsegm.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/types.h>

#define MINIMAP_WIDTH 300
#define MINIMAP_HEIGHT 160
#define CROP_HEIGHT 384
#define CROP_WIDTH 704

MotionDetector::MotionDetector(const std::string& ip, const std::string& username,
                               const std::string& password, int area, int w, int h, bool fullscreen)
    : m_motion_area(area),
      m_display_width(w),
      m_display_height(h),
      m_fullscreen(fullscreen),
      m_main_display(cv::Size(w, h), CV_8UC3, cv::Scalar(0, 0, 0)),
      m_sorted_chs_area_all({1, 2, 3, 4, 5, 6})
{

    // Initialize background subtractor
    // m_fgbg = cv::createBackgroundSubtractorMOG2(20, 32, true);
    m_fgbg = cv::createBackgroundSubtractorKNN(20, 400.0, true);
    // m_fgbg = cv::bgsegm::createBackgroundSubtractorCNT(true, 15, true);

    m_readers.push_back(std::make_unique<FrameReader>(0, ip, username, password));

    for (int channel = 1; channel <= 6; ++channel) {
        m_readers.push_back(std::make_unique<FrameReader>(channel, ip, username, password));
    }

    m_thread_detect_motion = std::thread([this]() { detect_motion(); });
}

void MotionDetector::do_tour_logic()
{
    m_tour_frame_index++;
    if (m_tour_frame_index >= TOUR_FRAME_COUNT) {
        m_tour_frame_index = 0;
        m_current_channel = m_current_channel % 6 + 1;
    }
}

std::vector<std::vector<cv::Point>> MotionDetector::find_contours_frame0()
{
    cv::Mat fgmask;
    m_fgbg->apply(m_frame0, fgmask);

    cv::Mat thresh;
    cv::threshold(fgmask, thresh, 128, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    return contours;
}

void MotionDetector::move_to_front(int value)
{
    auto list = m_sorted_chs_area_all.get();

    // Find the element
    auto it = std::find(list.begin(), list.end(), value);
    if (it != list.end()) {
        list.erase(it);         // Remove from current position (O(1) with iterator)
        list.push_front(value); // Insert at the beginning (O(1))
    }

    m_sorted_chs_area_all.update(list);
}

void MotionDetector::detect_largest_motion_area_set_channel()
{
    std::vector<std::vector<cv::Point>> contours = find_contours_frame0();

    // Find largest motion area
    cv::Rect m_motion_region;
    double max_area = 0;
    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= m_motion_area) {
            cv::Rect rect = cv::boundingRect(contour);
            cv::rectangle(m_frame0, rect, cv::Scalar(0, 255, 0), 1);
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
        cv::rectangle(m_frame0, m_motion_region, cv::Scalar(0, 0, 255), 2);
        m_tour_frame_index = 0; // reset so it doesn't auto switch on new tour so we can show a little bit of motion
        float rel_x = m_motion_region.x / static_cast<float>(CROP_WIDTH);
        float rel_y = m_motion_region.y / static_cast<float>(CROP_HEIGHT);
        int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);

        if (m_motion_ch != new_channel) { m_motion_ch = new_channel; }

        m_motion_ch_frames++;
        if (m_motion_detected_min_frames && m_current_channel != new_channel) {
            m_current_channel = new_channel;
            move_to_front(m_current_channel);
        }
    }
    else {
        m_motion_ch_frames = 0;
    }

    m_motion_detected_min_frames = m_motion_ch_frames >= MOTION_DETECT_MIN_FRAMES;

    m_frame0_dbuff.update(m_frame0);
}

void MotionDetector::draw_minimap()
{
    cv::Mat frame0 = m_frame0_dbuff.get();
    if (frame0.empty()) { return; } 

    cv::Mat minimap;
    cv::resize(frame0, minimap, cv::Size(MINIMAP_WIDTH, MINIMAP_HEIGHT));

    // Add white border
    cv::Mat minimap_padded;
    cv::copyMakeBorder(minimap, minimap_padded, 2, 2, 2, 2, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

    // Place minimap on main display
    cv::Point minimap_pos = cv::Point(10, 10);
    minimap_padded.copyTo(m_main_display(cv::Rect(minimap_pos.x, minimap_pos.y, minimap_padded.cols, minimap_padded.rows)));
}

void MotionDetector::draw_info()
{
    const int text_y_start = 200;
    const int text_y_step = 35;
    const cv::Scalar text_color(255, 255, 255);
    const double font_scale = 0.8;
    const int font_thickness = 2;
    int i = 0;

    cv::putText(m_main_display, "Info (i): " + bool_to_str(m_enableInfo),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Motion (m/n/l/s/k/x): " + bool_to_str(m_enableMotion) + "/" + std::to_string(m_motionDisplayMode),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Minimap (o/0): " + bool_to_str(m_enableMinimap) + "/" + bool_to_str(m_enableMinimapFullscreen),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Motion Detected: " + std::to_string(m_motion_detected) + " " + std::to_string(m_motion_detected_min_frames),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Channel (num): " + std::to_string(m_current_channel) + " <- " + std::to_string(m_motion_ch) + " " + std::to_string(m_motion_ch_frames) + "/" + std::to_string(MOTION_DETECT_MIN_FRAMES),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Fullscreen (f): " + bool_to_str(m_enableFullscreenChannel),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Tour (t): " + bool_to_str(m_enableTour) + " " + std::to_string(m_tour_frame_index) + "/" + std::to_string(TOUR_FRAME_COUNT),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Reset (r)",
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
}

cv::Mat MotionDetector::paint_main_mat_all() // need to fix this
{
    // // Assume readers[i] are objects that can get frames
    // for (int i = 0; i < 6; i++) {
    //     cv::Mat mat = m_readers[i + 1]->get_latest_frame();

    //     if (mat.empty()) {
    //         continue; // If the frame is empty, skip painting
    //     }

    //     // Compute position in the 3x2 grid
    //     int row = i / 3; // 0 for first row, 1 for second row
    //     int col = i % 3; // Column position (0,1,2)

    //     // Define the region of interest (ROI) in main_mat
    //     cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);

    //     // Copy the frame into main_mat at the correct position
    //     mat.copyTo(m_main_display(roi));
    // }

    return m_main_display;
}

cv::Mat MotionDetector::paint_main_mat_sort() // need to fix this
{

    // // Assume readers[i] are objects that can get frames
    // int i = 0;
    // for (int x : m_sorted_chs_area_all) {
    //     cv::Mat mat = m_readers[x]->get_latest_frame();

    //     if (mat.empty()) {
    //         continue; // If the frame is empty, skip painting
    //     }

    //     // Compute position in the 3x2 grid
    //     int row = i / 3; // 0 for first row, 1 for second row
    //     int col = i % 3; // Column position (0,1,2)
    //     i++;

    //     // Define the region of interest (ROI) in main_mat
    //     cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);

    //     // Copy the frame into main_mat at the correct position
    //     mat.copyTo(m_main_display(roi));
    // }

    return m_main_display;
}

cv::Mat MotionDetector::paint_main_mat_king()
{
    return paint_main_mat_king(m_sorted_chs_area_all.get());
}

cv::Mat MotionDetector::paint_main_mat_king(const std::list<int>& chs)
{
    std::cout << "--- ";
    for (const int& i : chs) { std::cout << i << " "; }
    std::cout << std::endl;

    size_t w = m_display_width / 3;
    size_t h = m_display_height / 3;

    int x = 1;
    for (int i : chs) {
        cv::Mat mat = m_readers[i]->get_latest_frame();
        if (mat.empty()) { continue; }

        cv::Rect roi;
        switch (x++) {
            case 1:
                cv::resize(mat, mat, cv::Size(w * 2, h * 2));
                roi = cv::Rect(0 * w, 0 * h, w * 2, h * 2);
                break;
            case 2:
                cv::resize(mat, mat, cv::Size(w, h));
                roi = cv::Rect(2 * w, 0 * h, w, h);
                break;
            case 3:
                cv::resize(mat, mat, cv::Size(w, h));
                roi = cv::Rect(2 * w, 1 * h, w, h);
                break;
            case 4:
                cv::resize(mat, mat, cv::Size(w, h));
                roi = cv::Rect(0 * w, 2 * h, w, h);
                break;
            case 5:
                cv::resize(mat, mat, cv::Size(w, h));
                roi = cv::Rect(1 * w, 2 * h, w, h);
                break;
            case 6:
                cv::resize(mat, mat, cv::Size(w, h));
                roi = cv::Rect(2 * w, 2 * h, w, h);
                break;
        }
        mat.copyTo(m_main_display(roi));
    }

    return m_main_display;
}

cv::Mat MotionDetector::paint_main_mat_top()
{

    std::list<int> chs;
    chs.emplace_back(m_current_channel);

    for (size_t i = 0; i < 6; i++) {
        int x = i + 1;
        if ((int)x == m_current_channel) continue;
        chs.emplace_back(x);
    }

    return paint_main_mat_king(chs);
}

void MotionDetector::handle_keys()
{
    // clang-format off
    char key = cv::waitKey(1);
    if (key == 'q') { stop(); }
    else if (key == 'm') { m_enableMotion = !m_enableMotion; }
    else if (key == 'n') { m_motionDisplayMode = MOTION_DISPLAY_MODE_NONE; }
    else if (key == 'l') { m_motionDisplayMode = MOTION_DISPLAY_MODE_LARGEST; }
    else if (key == 's') { m_motionDisplayMode = MOTION_DISPLAY_MODE_SORT; }
    else if (key == 'k') { m_motionDisplayMode = MOTION_DISPLAY_MODE_KING; }
    else if (key == 'x') { m_motionDisplayMode = MOTION_DISPLAY_MODE_TOP; }
    else if (key == 'i') { m_enableInfo = !m_enableInfo; }
    else if (key == 'o') { m_enableMinimap = !m_enableMinimap; }
    else if (key == 'f') { m_enableFullscreenChannel = !m_enableFullscreenChannel; }
    else if (key == 't') { m_enableTour = !m_enableTour; }
    else if (key == 'r') {
        m_current_channel = 1;
        m_motion_detected = false;
        m_tour_frame_index = 0;
        m_enableInfo = ENABLE_INFO;
        m_enableMotion = ENABLE_MOTION;
        m_enableMinimap = ENABLE_MINIMAP;
        m_enableFullscreenChannel = ENABLE_FULLSCREEN_CHANNEL;
        m_enableTour = ENABLE_TOUR;
    }
    else if (key == '0') {
        m_enableMinimapFullscreen = !m_enableMinimapFullscreen;
    }
    else if (key >= '1' && key <= '6') {
        m_current_channel = key - '0';
        m_enableFullscreenChannel = true;
    }
    // clang-format on
}

void MotionDetector::detect_motion()
{
    while (m_running) {

        m_motion_detected = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(MOTION_SLEEP_MS));
        if (!m_enableMotion) { continue; }

        cv::Mat frame0_get = m_readers[0]->get_latest_frame();
        if (frame0_get.empty()) { continue; }
        m_frame0 = frame0_get(cv::Rect(0, 0, CROP_WIDTH, CROP_HEIGHT));

        switch (m_motionDisplayMode) {
            case MOTION_DISPLAY_MODE_LARGEST:
            case MOTION_DISPLAY_MODE_SORT:
            case MOTION_DISPLAY_MODE_KING:
            case MOTION_DISPLAY_MODE_TOP:
                detect_largest_motion_area_set_channel();
                break;
        }
    }
}

void MotionDetector::draw_loop()
{
    m_running = true;

    if (m_fullscreen) {
        cv::namedWindow("Motion", cv::WINDOW_NORMAL);
        cv::setWindowProperty("Motion", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
    }

    try {
        while (m_running) {

            std::this_thread::sleep_for(std::chrono::milliseconds(DRAW_SLEEP_MS));

            cv::Mat main_frame_get = cv::Mat();

            if (m_enableTour) { do_tour_logic(); }

            if (m_enableMinimapFullscreen) {
                main_frame_get = m_frame0_dbuff.get();
            }
            else if (m_motionDisplayMode == MOTION_DISPLAY_MODE_SORT) {
                main_frame_get = paint_main_mat_sort();
            }
            else if (m_motionDisplayMode == MOTION_DISPLAY_MODE_KING) {
                main_frame_get = paint_main_mat_king();
            }
            else if (m_motionDisplayMode == MOTION_DISPLAY_MODE_TOP) {
                main_frame_get = paint_main_mat_top();
            }
            else if (m_enableFullscreenChannel ||
                     m_enableTour ||
                     (m_enableMotion && m_motionDisplayMode == MOTION_DISPLAY_MODE_LARGEST && m_motion_detected_min_frames)) {
                main_frame_get = m_readers[m_current_channel]->get_latest_frame();
            }
            else {
                main_frame_get = paint_main_mat_king();
            }

            if (!main_frame_get.empty()) {
                if (main_frame_get.size() != cv::Size(m_display_width, m_display_height)) {
                    cv::resize(main_frame_get, m_main_display, cv::Size(m_display_width, m_display_height));
                }
                else {
                    m_main_display = main_frame_get;
                }
            }

            if (m_enableMinimap) { draw_minimap(); }
            if (m_enableInfo) { draw_info(); }

            cv::imshow("Motion", m_main_display);

            handle_keys();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in display loop: " << e.what() << std::endl;
    }
}

void MotionDetector::stop()
{
    m_running = false;
    if (m_thread_detect_motion.joinable()) { m_thread_detect_motion.join(); }
    for (auto& reader : m_readers) { reader->stop(); }
    cv::destroyAllWindows();
}

std::string MotionDetector::bool_to_str(bool b)
{
    return std::string(b ? "Yes" : "No");
}
