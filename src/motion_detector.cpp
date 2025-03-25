#include "motion_detector.hpp"
#include "debug.h"
#include "globals.hpp"
#include "opencv2/core.hpp"
#include <exception>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/types.h>
#include <tuple>

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
    // Find the element
    auto it = std::find(m_sorted_chs_area_all.begin(), m_sorted_chs_area_all.end(), value);
    if (it != m_sorted_chs_area_all.end()) {
        m_sorted_chs_area_all.erase(it);         // Remove from current position (O(1) with iterator)
        m_sorted_chs_area_all.push_front(value); // Insert at the beginning (O(1))
    }
}

void MotionDetector::detect_largest_motion_area_set_channel()
{
    std::vector<std::vector<cv::Point>> contours = find_contours_frame0();

    // Find largest motion area
    double max_area = 0;
    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= m_motion_area) {
            cv::Rect rect = cv::boundingRect(contour);
            cv::rectangle(m_frame0_drawed, rect, cv::Scalar(0, 255, 0), 1);
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
        cv::rectangle(m_frame0_drawed, m_motion_region, cv::Scalar(0, 0, 255), 2);
        m_tour_frame_index = 0; // reset so it doesn't auto switch on new tour so we can show a little bit of motion
        float rel_x = m_motion_region.x / static_cast<float>(CROP_WIDTH);
        float rel_y = m_motion_region.y / static_cast<float>(CROP_HEIGHT);
        int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);

        assert(new_channel >= 1 && new_channel <= 6);

        if (m_motion_ch != new_channel) {
            m_motion_ch = new_channel;
        }

        m_motion_ch_frames++;
        if (m_motion_detected_min_frames && m_current_channel != new_channel) {
            m_current_channel = new_channel;
            move_to_front(m_current_channel);
            std::cout << "===" << std::endl;
            for (int i : m_sorted_chs_area_all)
                std::cout << i << std::endl;
            std::cout << "===" << std::endl;
        }
    }
    else {
        m_motion_ch_frames = 0;
    }

    m_motion_detected_min_frames = m_motion_ch_frames >= MOTION_DETECT_MIN_FRAMES;
}

void MotionDetector::sort_channels_by_motion_area_motion_channels()
{
    std::vector<std::vector<cv::Point>> contours = find_contours_frame0();

    double max_areas[6] = {0.0};
    m_sorted_chs_area_motion.clear();

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= m_motion_area) {
            cv::Rect rect = cv::boundingRect(contour);
            cv::rectangle(m_frame0_drawed, rect, cv::Scalar(0, 255, 0), 1);
            double area = rect.width * rect.height;

            float rel_x = rect.x / static_cast<float>(CROP_WIDTH);
            float rel_y = rect.y / static_cast<float>(CROP_HEIGHT);
            int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);

            if (area > max_areas[new_channel - 1]) {
                max_areas[new_channel - 1] = area;
            }
        }
    }

    for (int i = 0; i < 6; i++) {
        if (max_areas[i]) {
            m_sorted_chs_area_motion.emplace_back(std::make_pair(i + 1, max_areas[i]));
        }
    }

    // higher channel first
    std::sort(m_sorted_chs_area_motion.begin(), m_sorted_chs_area_motion.end(), [](auto& a, auto& b) { return a.second > b.second; });
}

void MotionDetector::draw_minimap()
{
    cv::Mat minimap;
    cv::resize(m_frame0_drawed, minimap, cv::Size(MINIMAP_WIDTH, MINIMAP_HEIGHT));

    // Add white border
    cv::Mat minimap_padded;
    cv::copyMakeBorder(minimap, minimap_padded, 2, 2, 2, 2, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

    // Place minimap on main display
    cv::Point minimap_pos = cv::Point(10, 10);
    minimap_padded.copyTo(m_main_c1r1(cv::Rect(minimap_pos.x, minimap_pos.y, minimap_padded.cols, minimap_padded.rows)));
}

void MotionDetector::draw_info()
{
    const int text_y_start = 200;
    const int text_y_step = 35;
    const cv::Scalar text_color(255, 255, 255);
    const double font_scale = 0.8;
    const int font_thickness = 2;
    int i = 0;

    cv::putText(m_main_c1r1, "Info (i): " + bool_to_str(m_enableInfo),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Motion (m/n/l/s/d/k/x): " + bool_to_str(m_enableMotion) + "/" + std::to_string(m_motionDisplayMode),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Minimap (o/0): " + bool_to_str(m_enableMinimap) + "/" + bool_to_str(m_enableMinimapFullscreen),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Motion Detected: " + std::to_string(m_motion_detected) + " " + std::to_string(m_motion_detected_min_frames),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Channel (num): " + std::to_string(m_current_channel) + " <- " + std::to_string(m_motion_ch) + " " + std::to_string(m_motion_ch_frames) + "/" + std::to_string(MOTION_DETECT_MIN_FRAMES),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Fullscreen (f): " + bool_to_str(m_enableFullscreenChannel),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Tour (t): " + bool_to_str(m_enableTour) + " " + std::to_string(m_tour_frame_index) + "/" + std::to_string(TOUR_FRAME_COUNT),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Reset (r)",
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
}

cv::Mat MotionDetector::paint_main_mat_all()
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
        mat.copyTo(m_main_c3r2(roi));
    }

    return m_main_c3r2;
}

cv::Mat MotionDetector::paint_main_mat_sort()
{
    // Assume readers[i] are objects that can get frames
    for (int x : m_sorted_chs_area_all) {
        int i = x - 1;
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
        mat.copyTo(m_main_c3r2(roi));
    }

    return m_main_c3r2;
}

cv::Mat MotionDetector::frame_get_non_empty(const int& i)
{

    cv::Mat frame = m_readers[i]->get_latest_frame();
    while (frame.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ERROR_SLEEP_MS));
        frame = m_readers[i]->get_latest_frame();
    }
    return frame;
}

cv::Mat MotionDetector::paint_main_mat_multi()
{
    switch (m_sorted_chs_area_motion.size()) {
    case 1:
        {
            return frame_get_non_empty(m_sorted_chs_area_motion[0].first);
            break;
        }
    case 2:
        {
            cv::Mat output_c1r2 = cv::Mat(cv::Size(W_HD * 1, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0));
            cv::Mat t = frame_get_non_empty(m_sorted_chs_area_motion[0].first);
            t.copyTo(output_c1r2(cv::Rect(0 * W_HD, 0 * H_HD, W_HD, H_HD)));
            cv::Mat b = frame_get_non_empty(m_sorted_chs_area_motion[1].first);
            b.copyTo(output_c1r2(cv::Rect(0 * W_HD, 1 * H_HD, W_HD, H_HD)));
            return output_c1r2;
            break;
        }
    case 3:
        {
            cv::Mat output_c1r2 = cv::Mat(cv::Size(W_HD * 2, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0));
            cv::Mat t = frame_get_non_empty(m_sorted_chs_area_motion[0].first);
            cv::resize(t, t, cv::Size(W_HD * 2, H_HD));
            t.copyTo(output_c1r2(cv::Rect(0 * W_HD, 0 * H_HD, W_HD * 2, H_HD)));
            cv::Mat bl = frame_get_non_empty(m_sorted_chs_area_motion[1].first);
            bl.copyTo(output_c1r2(cv::Rect(0 * W_HD, 1 * H_HD, W_HD, H_HD)));
            cv::Mat br = frame_get_non_empty(m_sorted_chs_area_motion[2].first);
            br.copyTo(output_c1r2(cv::Rect(1 * W_HD, 1 * H_HD, W_HD, H_HD)));
            return output_c1r2;

            break;
        }
    case 4:
        {
            cv::Mat output_c2r2 = cv::Mat(cv::Size(W_HD * 2, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0));
            cv::Mat tl = frame_get_non_empty(m_sorted_chs_area_motion[0].first);
            tl.copyTo(output_c2r2(cv::Rect(0 * W_HD, 0 * H_HD, W_HD, H_HD)));
            cv::Mat tr = frame_get_non_empty(m_sorted_chs_area_motion[1].first);
            tr.copyTo(output_c2r2(cv::Rect(1 * W_HD, 0 * H_HD, W_HD, H_HD)));
            cv::Mat bl = frame_get_non_empty(m_sorted_chs_area_motion[2].first);
            bl.copyTo(output_c2r2(cv::Rect(0 * W_HD, 1 * H_HD, W_HD, H_HD)));
            cv::Mat br = frame_get_non_empty(m_sorted_chs_area_motion[3].first);
            br.copyTo(output_c2r2(cv::Rect(1 * W_HD, 1 * H_HD, W_HD, H_HD)));
            return output_c2r2;
            break;
        }
    case 5:
        {
            cv::Mat out = cv::Mat(cv::Size(W_HD * 3, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0));
            cv::Mat big = frame_get_non_empty(m_sorted_chs_area_motion[0].first);
            cv::Mat resize;
            cv::resize(big, resize, cv::Size(W_HD * 2, H_HD));
            resize.copyTo(out(cv::Rect(0 * W_HD, 0 * H_HD, W_HD * 2, H_HD)));

            for (int i = 1; i < 5; i++) {
                int row = (i + 1) / 3;
                int col = (i + 1) % 3;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);
                frame_get_non_empty(m_sorted_chs_area_motion[i].first).copyTo(out(roi));
            }
            return out;

            break;
        }
    case 6:
        {
            for (size_t i = 0; i < 6; i++) {
                cv::Mat mat = frame_get_non_empty(m_sorted_chs_area_motion[i].first);
                int row = i / 3;
                int col = i % 3;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);
                mat.copyTo(m_main_c3r2(roi));
            }

            return m_main_c3r2;
            break;
        }
    }

    return paint_main_mat_all();
}

cv::Mat MotionDetector::paint_main_mat_king()
{
    cv::Mat output_c3r3 = cv::Mat(cv::Size(W_HD * 3, H_HD * 3), CV_8UC3, cv::Scalar(0, 0, 0));

    int x = 1;
    for (int i : m_sorted_chs_area_all) {
        cv::Mat mat = m_readers[i]->get_latest_frame();
        if (mat.empty()) { continue; }

        switch (x++) {

        case 1:
            {
                cv::resize(mat, mat, cv::Size(W_HD * 2, H_HD * 2));
                int row = 0;
                int col = 0;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD * 2, H_HD * 2);
                mat.copyTo(output_c3r3(roi));
                break;
            }
        case 2:
            {
                int row = 0;
                int col = 2;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);
                mat.copyTo(output_c3r3(roi));
                break;
            }
        case 3:
            {
                int row = 1;
                int col = 2;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);
                mat.copyTo(output_c3r3(roi));
                break;
            }
        case 4:
            {
                int row = 2;
                int col = 0;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);
                mat.copyTo(output_c3r3(roi));
                break;
            }
        case 5:
            {
                int row = 2;
                int col = 1;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);
                mat.copyTo(output_c3r3(roi));
                break;
            }
        case 6:
            {
                int row = 2;
                int col = 2;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);
                mat.copyTo(output_c3r3(roi));
                break;
            }
        }
    }

    return output_c3r3;
}

cv::Mat MotionDetector::paint_main_mat_top()
{

    m_sorted_chs_area_all.clear();
    m_sorted_chs_area_all.emplace_back(m_current_channel);

    for (size_t i = 0; i < 6; i++) {
        int x = i + 1;
        if ((int)x == m_current_channel) continue;
        m_sorted_chs_area_all.emplace_back(x);
    }

    return paint_main_mat_king();
}

void MotionDetector::start()
{
    m_running = true;

    if (m_fullscreen) {
        cv::namedWindow("Motion", cv::WINDOW_NORMAL);
        cv::setWindowProperty("Motion", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
    }

    try {

        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(DRAW_SLEEP_MS)); // Prevent CPU overuse

            cv::Mat frame0_get = m_readers[0]->get_latest_frame();
            if (!frame0_get.empty()) {
                m_frame0 = frame0_get(cv::Rect(0, 0, CROP_WIDTH, CROP_HEIGHT));
                m_frame0_drawed = m_frame0.clone();
            }

            cv::Mat main_frame_get = cv::Mat();

            if (m_enableTour) { do_tour_logic(); }

            m_motion_detected = false;
            if (m_enableMotion) {
                switch (m_motionDisplayMode) {
                case MOTION_DISPLAY_MODE_LARGEST:
                case MOTION_DISPLAY_MODE_SORT:
                case MOTION_DISPLAY_MODE_KING:
                case MOTION_DISPLAY_MODE_TOP:
                    detect_largest_motion_area_set_channel();
                    break;
                }
            }
            if (m_enableMotion && m_motionDisplayMode == MOTION_DISPLAY_MODE_MULTI) { sort_channels_by_motion_area_motion_channels(); }

            if (m_enableMinimapFullscreen) {
                main_frame_get = m_frame0_drawed;
            }
            else if (m_motionDisplayMode == MOTION_DISPLAY_MODE_SORT) {
                main_frame_get = paint_main_mat_sort();
            }
            else if (m_motionDisplayMode == MOTION_DISPLAY_MODE_MULTI) {
                main_frame_get = paint_main_mat_multi();
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
                main_frame_get = paint_main_mat_top();
            }

            if (!main_frame_get.empty()) { cv::resize(main_frame_get, m_main_c1r1, cv::Size(m_display_width, m_display_height)); }

            if (m_enableMinimap) { draw_minimap(); }
            if (m_enableInfo) { draw_info(); }

            cv::imshow("Motion", m_main_c1r1);

            // clang-format off
            char key = cv::waitKey(1);
            if (key == 'q') { stop(); break; }
            else if (key == 'm') { m_enableMotion = !m_enableMotion; }
            else if (key == 'n') { m_motionDisplayMode = MOTION_DISPLAY_MODE_NONE; }
            else if (key == 'l') { m_motionDisplayMode = MOTION_DISPLAY_MODE_LARGEST; }
            else if (key == 's') { m_motionDisplayMode = MOTION_DISPLAY_MODE_SORT; }
            else if (key == 'd') { m_motionDisplayMode = MOTION_DISPLAY_MODE_MULTI; }
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
    }
    catch (const std::exception& e) {
        std::cerr << "Error in display loop: " << e.what() << std::endl;
    }
}

void MotionDetector::stop()
{
    m_running = false;

    DPL("stopping all readers");
    for (auto& reader : m_readers) {
        reader->stop();
    }
    DPL("stopped all readers");

    DPL("close all wins");
    cv::destroyAllWindows();
    DPL("closed all wins");
}

std::string MotionDetector::bool_to_str(bool b)
{
    return std::string(b ? "Yes" : "No");
}
