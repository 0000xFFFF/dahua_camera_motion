#include "motion_detector.hpp"
#include "globals.hpp"
#include "opencv2/core.hpp"
#include <exception>
#include <iostream>
#include <opencv2/bgsegm.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/types.h>
#include <unistd.h>

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
      m_frame0(cv::Mat::zeros(W_0, H_0, CV_8UC3)),
      m_canv3x3(cv::Mat(cv::Size(w, h), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_canv3x2(cv::Mat(cv::Size(w, h), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_main_display(cv::Mat(cv::Size(w, h), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_king_chain({1, 2, 3, 4, 5, 6})

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

void MotionDetector::change_channel(int ch)
{
    move_to_front(ch);
    m_current_channel = ch;
}

void MotionDetector::do_tour_logic()
{
    m_tour_frame_index++;
    if (m_tour_frame_index >= m_tour_frame_count) {
        m_tour_frame_index = 0;
#if KING_LAYOUT == 1
        change_channel(m_king_chain.get().back());
#else
        m_tour_current_channel++;
        if (m_tour_current_channel > 6) { m_tour_current_channel = 1; }
        change_channel(m_tour_current_channel);
#endif
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
    auto list = m_king_chain.get();

    // Find the element
    auto it = std::find(list.begin(), list.end(), value);
    if (it != list.end()) {
        list.erase(it);         // Remove from current position (O(1) with iterator)
        list.push_front(value); // Insert at the beginning (O(1))
    }

    m_king_chain.update(list);
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
        float rel_x = m_motion_region.x / static_cast<float>(CROP_WIDTH);
        float rel_y = m_motion_region.y / static_cast<float>(CROP_HEIGHT);
        int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);

        if (m_motion_ch != new_channel) { m_motion_ch = new_channel; }

        m_motion_ch_frames++;
        if (m_motion_detected_min_frames && m_current_channel != new_channel) {
            change_channel(new_channel);
        }
    }
    else {
        m_motion_ch_frames = 0;
    }

    m_motion_detect_min_frames = (m_motion_sleep_ms > 0) ? MOTION_DETECT_MIN_MS / m_motion_sleep_ms : MOTION_DETECT_MIN_MS / 10;
    m_motion_detected_min_frames = m_motion_ch_frames >= m_motion_detect_min_frames;

    if (m_motion_detected_min_frames) {
        m_tour_frame_index = 0; // reset so it doesn't auto switch on new tour so we can show a little bit of motion
    }

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
    cv::putText(m_main_display, "Display Mode (n/a/s/k/x): " + std::to_string(m_displayMode),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Motion (m/l): " + bool_to_str(m_enableMotion) + "/" + bool_to_str(m_enableMotionZoomLargest),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Minimap (o/0): " + bool_to_str(m_enableMinimap) + "/" + bool_to_str(m_enableMinimapFullscreen),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Motion Detected: " + std::to_string(m_motion_detected) + " " + std::to_string(m_motion_detected_min_frames),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Channel (num): " + std::to_string(m_current_channel) + " <- " + std::to_string(m_motion_ch) + " " + std::to_string(m_motion_ch_frames) + "/" + std::to_string(m_motion_detect_min_frames),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Fullscreen (f): " + bool_to_str(m_enableFullscreenChannel),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Tour (t): " + bool_to_str(m_enableTour) + " " + std::to_string(m_tour_frame_index) + "/" + std::to_string(m_tour_frame_count) + " = " + std::to_string(m_tour_current_channel),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Reset (r)",
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
}

cv::Mat MotionDetector::paint_main_mat_all()
{
    return paint_main_mat_all({1, 2, 3, 4, 5, 6});
}

cv::Mat MotionDetector::paint_main_mat_all(const std::list<int>& chs)
{
    size_t w = m_display_width / 3;
    size_t h = m_display_height / 2;

    int x = 0;
    for (const int& i : chs) {
        x++;

        cv::Mat mat = m_readers[i]->get_latest_frame();
        if (mat.empty()) { continue; }

        cv::resize(mat, mat, cv::Size(w, h));

        // Compute position in the 3x2 grid
        int row = (x - 1) / 3;
        int col = (x - 1) % 3;
        cv::Rect roi(col * w, row * h, w, h);
        mat.copyTo(m_canv3x2(roi));
    }

    return m_canv3x2;
}

cv::Mat MotionDetector::paint_main_mat_sort() // need to fix this
{
    return paint_main_mat_all(m_king_chain.get());
}

cv::Mat MotionDetector::paint_main_mat_king()
{
    return paint_main_mat_king(m_king_chain.get());
}

#define KING_LAYOUT_REL 1
#define KING_LAYOUT_CIRC 2

#ifndef KING_LAYOUT
#define KING_LAYOUT 1
#endif
cv::Mat MotionDetector::paint_main_mat_king(const std::list<int>& chs)
{
    size_t w = m_display_width / 3;
    size_t h = m_display_height / 3;

    int x = 0;
    for (const int& i : chs) {
        x++;
        cv::Mat mat = m_readers[i]->get_latest_frame();
        if (mat.empty()) { continue; }

        cv::Rect roi;

#if KING_LAYOUT == KING_LAYOUT_REL
        switch (x) {
                // clang-format off
            case 1: cv::resize(mat, mat, cv::Size(w * 2, h * 2)); roi = cv::Rect(0 * w, 0 * h, w * 2, h * 2); break;
            case 2: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(2 * w, 0 * h, w, h);         break;
            case 3: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(2 * w, 1 * h, w, h);         break;
            case 4: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(0 * w, 2 * h, w, h);         break;
            case 5: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(1 * w, 2 * h, w, h);         break;
            case 6: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(2 * w, 2 * h, w, h);         break;
                // clang-format on
        }
#endif

#if KING_LAYOUT == KING_LAYOUT_CIRC
        // CIRCLE LAYOUR
        switch (x) {
                // clang-format off
            case 1: cv::resize(mat, mat, cv::Size(w * 2, h * 2)); roi = cv::Rect(0 * w, 0 * h, w * 2, h * 2); break;
            case 2: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(2 * w, 0 * h, w, h);         break;
            case 3: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(2 * w, 1 * h, w, h);         break;
            case 4: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(2 * w, 2 * h, w, h);         break;
            case 5: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(1 * w, 2 * h, w, h);         break;
            case 6: cv::resize(mat, mat, cv::Size(w, h));         roi = cv::Rect(0 * w, 2 * h, w, h);         break;
                // clang-format on
        }
#endif

        mat.copyTo(m_canv3x3(roi));
    }

    return m_canv3x3;
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
    int key = cv::waitKey(1);
    if (key == 'q') { stop(); }
    else if (key == 'm') { m_enableMotion = !m_enableMotion; }
    else if (key == 'l') { m_enableMotionZoomLargest = !m_enableMotionZoomLargest; }
    else if (key == 'n') { m_displayMode = DISPLAY_MODE_SINGLE; }
    else if (key == 'a') { m_displayMode = DISPLAY_MODE_ALL; }
    else if (key == 's') { m_displayMode = DISPLAY_MODE_SORT; }
    else if (key == 'x') { m_displayMode = DISPLAY_MODE_TOP; }
    else if (key == 'k') { m_displayMode = DISPLAY_MODE_KING; }
    else if (key == 'i') { m_enableInfo = !m_enableInfo; }
    else if (key == 'o') { m_enableMinimap = !m_enableMinimap; }
    else if (key == 'f') { m_enableFullscreenChannel = !m_enableFullscreenChannel; }
    else if (key == 't') { m_enableTour = !m_enableTour; }
    else if (key == 'r') {
        m_current_channel = 1;
        m_king_chain.update({1, 2, 3, 4, 5, 6});
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
    else if (key == 81 || key == 65361) { // Left arrow (81 on Windows, 65361 on Linux)
        int new_ch = m_current_channel + 1;
        if (new_ch > 6) new_ch = 1;
        change_channel(new_ch);
    } else if (key == 83 || key == 65363) { // Right arrow (83 on Windows, 65363 on Linux)
        int new_ch = m_current_channel - 1;
        if (new_ch < 1) new_ch = 6;
        change_channel(new_ch);
    }

    else if (key >= '1' && key <= '6') {
        change_channel(key - '0');
    }
    // clang-format on
}

void MotionDetector::detect_motion()
{

#ifdef SLEEP_MS_MOTION
    m_motion_sleep_ms = SLEEP_MS_MOTION;
#endif

#ifdef DEBUG_VERBOSE
    int i = 0;
#endif
    while (m_running) {

#ifdef DEBUG_VERBOSE
        i++;
#endif

#ifndef SLEEP_MS_MOTION
        auto motion_start = std::chrono::high_resolution_clock::now();
#endif

        m_motion_detected = false;
        if (m_enableMotion) {
            cv::Mat frame0_get = m_readers[0]->get_latest_frame();
            if (!frame0_get.empty()) {
                m_frame0 = frame0_get(cv::Rect(0, 0, CROP_WIDTH, CROP_HEIGHT));
                detect_largest_motion_area_set_channel();
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

#ifndef SLEEP_MS_MOTION
        // Calculate sleep time based on measured FPS
        double fps = m_readers[0]->get_fps();                        // get fps from any 1-6 ch they all have the same fps
        double detect_time = (fps > 0.0) ? (1.0 / fps) : 1.0 / 30.0; // Default to 30 FPS if zero
        auto motion_time = std::chrono::high_resolution_clock::now() - motion_start;
        auto sleep_time = std::chrono::duration<double>(detect_time) - motion_time;
        m_motion_sleep_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time).count();
#endif

#ifdef DEBUG_VERBOSE
        if (i % 300 == 0) {
            std::cout << "Motion thread sleep time: " << m_motion_sleep_ms << " ms" << std::endl;
        }
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(m_motion_sleep_ms));
    }
}

void MotionDetector::draw_loop()
{
    m_running = true;

    if (m_fullscreen) {
        cv::namedWindow("Motion", cv::WINDOW_NORMAL);
        cv::setWindowProperty("Motion", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
    }

#ifdef SLEEP_MS_DRAW
    m_draw_sleep_ms = SLEEP_MS_DRAW;
    m_tour_frame_count = SLEEP_MS_TOUR / SLEEP_MS_DRAW;
#endif

    try {

#ifdef DEBUG_VERBOSE
        int i = 0;
#endif
        while (m_running) {

#ifdef DEBUG_VERBOSE
            i++;
#endif

#ifndef SLEEP_MS_DRAW
            auto draw_start = std::chrono::high_resolution_clock::now();
#endif

            if (m_enableTour) { do_tour_logic(); }

            cv::Mat get;
            if (m_enableMinimapFullscreen) {
                get = m_frame0_dbuff.get();
            }
            else if (m_enableFullscreenChannel ||
                     (m_displayMode == DISPLAY_MODE_SINGLE) ||
                     (m_enableMotion && m_enableMotionZoomLargest && m_motion_detected_min_frames)) {
                get = m_readers[m_current_channel]->get_latest_frame();
            }
            else if (m_displayMode == DISPLAY_MODE_SORT) {
                get = paint_main_mat_sort();
            }
            else if (m_displayMode == DISPLAY_MODE_KING) {
                get = paint_main_mat_king();
            }
            else if (m_displayMode == DISPLAY_MODE_TOP) {
                get = paint_main_mat_top();
            }
            else if (m_displayMode == DISPLAY_MODE_ALL) {
                get = paint_main_mat_all();
            }

            if (!get.empty()) {
                if (get.size() != cv::Size(m_display_width, m_display_height)) {
                    cv::resize(get, m_main_display, cv::Size(m_display_width, m_display_height));
                }
                else {
                    m_main_display = get;
                }
            }

            if (m_enableMinimap) { draw_minimap(); }
            if (m_enableInfo) { draw_info(); }

            cv::imshow("Motion", m_main_display);

            handle_keys();

#ifndef SLEEP_MS_DRAW
            // Calculate sleep time based on measured FPS
            double fps = m_readers[1]->get_fps();                       // get fps from any 1-6 ch they all have the same fps
            double frame_time = (fps > 0.0) ? (1.0 / fps) : 1.0 / 30.0; // Default to 30 FPS if zero
            auto draw_time = std::chrono::high_resolution_clock::now() - draw_start;

            auto sleep_time = std::chrono::duration<double>(frame_time) - draw_time;
            m_draw_sleep_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time).count();
            m_tour_frame_count = (m_draw_sleep_ms > 0) ? SLEEP_MS_TOUR / m_draw_sleep_ms : SLEEP_MS_TOUR / 10;
#endif

#ifdef DEBUG_VERBOSE
            if (i % 300 == 0) {
                std::cout << "Draw thread sleep time: " << m_draw_sleep_ms << " ms" << std::endl;
            }
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(m_draw_sleep_ms));
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
