#include "motion_detector.hpp"
#include "debug.hpp"
#include "globals.hpp"
#include "opencv2/core.hpp"
#include <any>
#include <exception>
#include <iostream>
#include <opencv2/bgsegm.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#define MINIMAP_WIDTH 300
#define MINIMAP_HEIGHT 160
#define CROP_WIDTH 704
#define CROP_HEIGHT 384

MotionDetector::MotionDetector(const std::string& ip,
                               const std::string& username,
                               const std::string& password,
                               int width,
                               int height,
                               bool fullscreen,
                               int display_mode,
                               int area,
                               int rarea,
                               int current_channel,
                               int enable_motion,
                               int enable_motion_zoom_largest,
                               int enable_tour,
                               int enable_info,
                               int enable_info_line,
                               int enable_info_rect,
                               int enable_minimap,
                               int enable_minimap_fullscreen,
                               int enable_fullscreen_channel)
    :

      m_display_width(width),
      m_display_height(height),
      m_fullscreen(fullscreen),
      m_display_mode(display_mode),
      m_motion_min_area(area),
      m_motion_min_rect_area(rarea),
      m_enable_motion(enable_motion),
      m_enable_motion_zoom_largest(enable_motion_zoom_largest),
      m_enable_tour(enable_tour),
      m_enable_info(enable_info),
      m_enable_info_line(enable_info_line),
      m_enable_info_rect(enable_info_rect),
      m_enable_minimap(enable_minimap),
      m_enable_minimap_fullscreen(enable_minimap_fullscreen),
      m_enable_fullscreen_channel(enable_fullscreen_channel),
      m_canv3x3(cv::Mat(cv::Size(width, height), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_canv3x2(cv::Mat(cv::Size(width, height), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_main_display(cv::Mat(cv::Size(width, height), CV_8UC3, cv::Scalar(0, 0, 0)))

{

    // m_fgbg = cv::createBackgroundSubtractorMOG2(20, 32, true);
    m_fgbg = cv::createBackgroundSubtractorKNN(20, 400.0, true);
    // m_fgbg = cv::bgsegm::createBackgroundSubtractorCNT(true, 15, true);

    m_readers.push_back(std::make_unique<FrameReader>(0, ip, username, password));

    for (int channel = 1; channel <= 6; ++channel) {
        m_readers.push_back(std::make_unique<FrameReader>(channel, ip, username, password));
    }

    m_thread_detect_motion = std::thread([this]() { detect_motion(); });

    change_channel(current_channel);
}

void MotionDetector::change_channel(int ch)
{

#ifdef SLEEP_MS_FRAME
    for (int i = 1; i <= 6; i++) {
        if (ch == i) {
            m_readers[i]->disable_sleep();
        }
        else {
            m_readers[i]->enable_sleep();
        }
    }
#endif

    move_to_front(ch);
    m_current_channel = ch;
}

void MotionDetector::do_tour_logic()
{
    if (!m_tour_start_set) {
        m_tour_start = std::chrono::high_resolution_clock::now();
        m_tour_start_set = true;
    }

    auto currentTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_tour_start).count();

    if (elapsed >= TOUR_MS) {
        m_tour_start = std::chrono::high_resolution_clock::now();
#if KING_LAYOUT == KING_LAYOUT_CIRC
        change_channel(m_king_chain.get().back());
#else

#if KING_LAYOUT == KING_LAYOUT_REL

        m_tour_current_channel++;
        if (m_tour_current_channel > 6) { m_tour_current_channel = 1; }

        int ch = m_tour_current_channel;
        auto vec = m_king_chain.get();
        std::vector<int> n(vec.size());
        int x = 0;
        n[x++] = ch;
        for (int i = 0; i < 6; i++) {
            int value = i + 1;
            if (value != ch) {
                n[x++] = value;
            }
        }
        m_king_chain.update(n);
        m_current_channel = ch;
#else
        // fight to top
        m_tour_current_channel++;
        if (m_tour_current_channel > 6) { m_tour_current_channel = 1; }
        change_channel(m_tour_current_channel);
#endif
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

void MotionDetector::move_to_front(int ch)
{
    auto vec = m_king_chain.get();
    std::vector<int> new_vec(vec.size());

    int x = 0;
    new_vec[x++] = ch;
    for (size_t i = 0; i < vec.size(); i++) {
        int value = vec[i];
        if (value != ch) {
            new_vec[x++] = value;
        }
    }
    m_king_chain.update(new_vec);
}

void MotionDetector::detect_largest_motion_area_set_channel()
{
    std::vector<std::vector<cv::Point>> contours = find_contours_frame0();

    // Find largest motion area
    cv::Rect motion_region;
    double max_area = 0;
    m_motion_detected = false;

    if (m_enable_minimap || m_enable_minimap_fullscreen)
        cv::drawContours(m_frame0, contours, -1, cv::Scalar(255, 0, 0), 1);

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= m_motion_min_area) {
            cv::Rect rect = cv::boundingRect(contour);
            double area = rect.width * rect.height;
            if (area >= m_motion_min_rect_area) {
                if (m_enable_minimap || m_enable_minimap_fullscreen)
                    cv::rectangle(m_frame0, rect, cv::Scalar(0, 255, 0), 1);

                if (area > max_area) {
                    max_area = area;
                    motion_region = rect;
                    m_motion_detected = true;
                }
            }
        }
    }

    auto now = std::chrono::high_resolution_clock::now();
    if (m_motion_detected) {

        if (m_enable_minimap || m_enable_minimap_fullscreen)
            cv::rectangle(m_frame0, motion_region, cv::Scalar(0, 0, 255), 2);

        m_motion_region.push(motion_region);

        if (!m_motion_detect_start_set) {
            m_motion_detect_start = now;
            m_motion_detect_start_set = true;
        }

        auto motion_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_motion_detect_start).count();
        m_motion_detected_min_ms = motion_duration >= MOTION_DETECT_MIN_MS;
        if (m_motion_detected_min_ms) {
            float rel_x = motion_region.x / static_cast<float>(CROP_WIDTH);
            float rel_y = motion_region.y / static_cast<float>(CROP_HEIGHT);
            int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);
            if (m_current_channel != new_channel) {
                change_channel(new_channel);
            }
            m_motion_detect_linger_start = now;
            m_motion_detect_linger = true;
        }
    }
    else {
        m_motion_detect_start_set = false;
        m_motion_detected_min_ms = false;
    }

    if (m_motion_detect_linger) {
        auto linger_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_motion_detect_linger_start).count();
        if (linger_elapsed >= MOTION_DETECT_LINGER_MS) {
            m_motion_detect_linger = false;
        }
    }

    if (m_enable_minimap || m_enable_minimap_fullscreen)
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

    cv::putText(m_main_display, "Info (i): " + bool_to_str(m_enable_info),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Display Mode (n/a/s/k/x): " + std::to_string(m_display_mode),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Motion (m/l): " + bool_to_str(m_enable_motion) + "/" + bool_to_str(m_enable_motion_zoom_largest),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Minimap (o/0): " + bool_to_str(m_enable_minimap) + "/" + bool_to_str(m_enable_minimap_fullscreen),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Motion Detected: " + std::to_string(m_motion_detected) + " " + std::to_string(m_motion_detected_min_ms),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Channel (num): " + std::to_string(m_current_channel),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Fullscreen (f): " + bool_to_str(m_enable_fullscreen_channel),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Tour (t): " + bool_to_str(m_enable_tour),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Reset (r)",
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
}

void MotionDetector::draw_info_line()
{
    if (m_motion_detected_min_ms) {
        cv::rectangle(m_main_display, cv::Rect(0, 0, m_main_display.size().width - 1, m_main_display.size().height - 1), cv::Scalar(0, 0, 255), 1, cv::LINE_8);
    }
    else if (m_motion_detect_linger) {
        cv::rectangle(m_main_display, cv::Rect(0, 0, m_main_display.size().width - 1, m_main_display.size().height - 1), cv::Scalar(0, 255, 255), 1, cv::LINE_8);
    }
    else if (m_enable_tour) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_tour_start).count();
        float p = (float)elapsed / TOUR_MS;
        if (p > 100) { p = 100; }
        cv::rectangle(m_main_display, cv::Rect(0, 0, m_main_display.size().width - 1, m_main_display.size().height - 1), cv::Scalar(0, 0, 0), 1, cv::LINE_8);

        int w = m_main_display.size().width - 1;
        int h = m_main_display.size().height - 1;
        cv::line(m_main_display, cv::Point(0, 0), cv::Point(p * w, 0), cv::Scalar(0, 165, 255), 1, cv::LINE_8);
        cv::line(m_main_display, cv::Point(w, 0), cv::Point(w, p * h), cv::Scalar(0, 165, 255), 1, cv::LINE_8);
        cv::line(m_main_display, cv::Point(w, h), cv::Point(w - p * w, h), cv::Scalar(0, 165, 255), 1, cv::LINE_8);
        cv::line(m_main_display, cv::Point(0, h), cv::Point(0, h - p * h), cv::Scalar(0, 165, 255), 1, cv::LINE_8);
    }
    else {
        cv::rectangle(m_main_display, cv::Rect(0, 0, m_main_display.size().width - 1, m_main_display.size().height - 1), cv::Scalar(0, 0, 0), 1, cv::LINE_8);
    }
}

cv::Mat MotionDetector::paint_main_mat_all()
{
    size_t w = m_display_width / 3;
    size_t h = m_display_height / 2;

    cv::parallel_for_(cv::Range(0, 6), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            int ch = i + 1;
            cv::Mat mat = m_readers[ch]->get_latest_frame();
            if (mat.empty()) { continue; }

            int row = i / 3;
            int col = i % 3;
            int x = col * w;
            int y = row * h;
            cv::resize(mat, m_canv3x2(cv::Rect(x, y, w, h)), cv::Size(w, h));
            if (ch == m_current_channel) {
                draw_motion_region(m_canv3x2, x, y, w, h);
            }
        }
    });

    return m_canv3x2;
}

cv::Mat MotionDetector::paint_main_mat_sort() // need to fix this
{
    size_t w = m_display_width / 3;
    size_t h = m_display_height / 2;

    std::vector vec = m_king_chain.get();

    cv::parallel_for_(cv::Range(0, 6), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            int ch = vec[i];
            cv::Mat mat = m_readers[ch]->get_latest_frame();
            if (mat.empty()) { continue; }

            int row = i / 3;
            int col = i % 3;
            int x = col * w;
            int y = row * h;
            cv::Rect roi(x, y, w, h);
            cv::resize(mat, m_canv3x2(cv::Rect(x, y, w, h)), cv::Size(w, h));
            if (ch == m_current_channel) {
                draw_motion_region(m_canv3x2, x, y, w, h);
            }
        }
    });

    return m_canv3x2;
}

void MotionDetector::draw_motion_region(cv::Mat canv, size_t posX, size_t posY, size_t width, size_t height)
{
    if (!m_enable_info_rect || !m_motion_detected_min_ms) { return; }
    auto opt_region = m_motion_region.pop();
    if (opt_region) {
        int ch = m_current_channel;
        cv::Rect region = *opt_region;
        int row = (ch - 1) / 3;
        int col = (ch - 1) % 3;

        constexpr int mini_ch_w = CROP_WIDTH / 3;
        constexpr int mini_ch_h = CROP_HEIGHT / 2;

        float scaleX = static_cast<float>(width) / mini_ch_w;
        float scaleY = static_cast<float>(height) / mini_ch_h;

        int offsetX = col * mini_ch_w;
        int offsetY = row * mini_ch_h;

        cv::Rect new_motion_region(
            (region.x - offsetX) * scaleX + posX,
            (region.y - offsetY) * scaleY + posY,
            region.width * scaleX,
            region.height * scaleY);

        cv::rectangle(canv, new_motion_region, cv::Scalar(0, 0, 255), 2);
    }
}

cv::Mat MotionDetector::paint_main_mat_king()
{
    size_t w = m_display_width / 3;
    size_t h = m_display_height / 3;

    std::vector vec = m_king_chain.get();

    cv::parallel_for_(cv::Range(0, 6), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            cv::Mat mat = m_readers[vec[i]]->get_latest_frame();
            if (mat.empty()) { continue; }

#if KING_LAYOUT == KING_LAYOUT_REL
            switch (i) {
                case 0:
                    {
                        size_t w0 = w * 2;
                        size_t h0 = h * 2;
                        cv::resize(mat, m_canv3x3(cv::Rect(0 * w, 0 * h, w0, h0)), cv::Size(w0, h0));
                        draw_motion_region(m_canv3x3(cv::Rect(0 * w, 0 * h, w0, h0)), 0, 0, w0, h0);
                        break;
                    }
                case 1: cv::resize(mat, m_canv3x3(cv::Rect(2 * w, 0 * h, w, h)), cv::Size(w, h)); break;
                case 2: cv::resize(mat, m_canv3x3(cv::Rect(2 * w, 1 * h, w, h)), cv::Size(w, h)); break;
                case 3: cv::resize(mat, m_canv3x3(cv::Rect(0 * w, 2 * h, w, h)), cv::Size(w, h)); break;
                case 4: cv::resize(mat, m_canv3x3(cv::Rect(1 * w, 2 * h, w, h)), cv::Size(w, h)); break;
                case 5: cv::resize(mat, m_canv3x3(cv::Rect(2 * w, 2 * h, w, h)), cv::Size(w, h)); break;
            }
#endif

#if KING_LAYOUT == KING_LAYOUT_CIRC
            // CIRCLE LAYOUR
            switch (i) {
                case 0:
                    {
                        size_t w0 = w * 2;
                        size_t h0 = h * 2;
                        cv::resize(mat, m_canv3x3(cv::Rect(0 * w, 0 * h, w0, h0)), cv::Size(w0, h0));
                        draw_motion_region(m_canv3x3, 0, 0, w0, h0);
                        break;
                    }
                case 1: cv::resize(mat, m_canv3x3(cv::Rect(2 * w, 0 * h, w, h)), cv::Size(w, h)); break;
                case 2: cv::resize(mat, m_canv3x3(cv::Rect(2 * w, 1 * h, w, h)), cv::Size(w, h)); break;
                case 3: cv::resize(mat, m_canv3x3(cv::Rect(2 * w, 2 * h, w, h)), cv::Size(w, h)); break;
                case 4: cv::resize(mat, m_canv3x3(cv::Rect(1 * w, 2 * h, w, h)), cv::Size(w, h)); break;
                case 5: cv::resize(mat, m_canv3x3(cv::Rect(0 * w, 2 * h, w, h)), cv::Size(w, h)); break;
            }
#endif
        }
    });

    return m_canv3x3;
}

cv::Mat MotionDetector::paint_main_mat_top()
{
    size_t w = m_display_width / 3;
    size_t h = m_display_height / 3;

    std::vector<int> active_channels;

    // Fill active_channels with [1,2,3,4,5,6] excluding m_current_channel
    for (int i = 1; i <= 6; i++) {
        if (i != m_current_channel) {
            active_channels.push_back(i);
        }
    }

    cv::parallel_for_(cv::Range(0, 6), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            cv::Mat mat;
            cv::Rect roi;

            if (i == 0) {
                mat = m_readers[m_current_channel]->get_latest_frame();
                if (mat.empty()) { continue; }

                size_t w0 = w * 2;
                size_t h0 = h * 2;
                cv::resize(mat, m_canv3x3(cv::Rect(0 * w, 0 * h, w0, h0)), cv::Size(w0, h0));
                draw_motion_region(m_canv3x3, 0, 0, w0, h0);
            }
            else {
                // Other slots are from active_channels (excluding m_current_channel)
                int channel = active_channels[i - 1]; // Skip 0th slot
                mat = m_readers[channel]->get_latest_frame();
                if (mat.empty()) { continue; }

                // Layout for circular arrangement
                switch (i) {
                    case 1: roi = cv::Rect(2 * w, 0 * h, w, h); break;
                    case 2: roi = cv::Rect(2 * w, 1 * h, w, h); break;
                    case 3: roi = cv::Rect(2 * w, 2 * h, w, h); break;
                    case 4: roi = cv::Rect(1 * w, 2 * h, w, h); break;
                    case 5: roi = cv::Rect(0 * w, 2 * h, w, h); break;
                }

                cv::resize(mat, m_canv3x3(roi), cv::Size(w, h));
            }
        }
    });

    return m_canv3x3;
}

void MotionDetector::handle_keys()
{
    // clang-format off
    int key = cv::waitKey(1);
    if (key == 'q') { stop(); }
    else if (key == 'm') { m_enable_motion = !m_enable_motion; }
    else if (key == 'l') { m_enable_motion_zoom_largest = !m_enable_motion_zoom_largest; }
    else if (key == 'n') { m_display_mode = DISPLAY_MODE_SINGLE; }
    else if (key == 'a') { m_display_mode = DISPLAY_MODE_ALL; }
    else if (key == 's') { m_display_mode = DISPLAY_MODE_SORT; }
    else if (key == 'x') { m_display_mode = DISPLAY_MODE_TOP; }
    else if (key == 'k') { m_display_mode = DISPLAY_MODE_KING; }
    else if (key == 'i') { m_enable_info = !m_enable_info; }
    else if (key == 'o') { m_enable_minimap = !m_enable_minimap; }
    else if (key == 'f') { m_enable_fullscreen_channel = !m_enable_fullscreen_channel; }
    else if (key == 't') { m_enable_tour = !m_enable_tour; }
    else if (key == 'r') {
        m_current_channel = 1;
        m_king_chain.update({1, 2, 3, 4, 5, 6});
        m_motion_detected = false;
        m_enable_info = ENABLE_INFO;
        m_enable_motion = ENABLE_MOTION;
        m_enable_minimap = ENABLE_MINIMAP;
        m_enable_fullscreen_channel = ENABLE_FULLSCREEN_CHANNEL;
        m_enable_tour = ENABLE_TOUR;
    }
    else if (key == '0') {
        m_enable_minimap_fullscreen = !m_enable_minimap_fullscreen;
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

#ifdef DEBUG_FPS
    int i = 0;
#endif

#ifdef SLEEP_MS_FRAME
    m_readers[0]->disable_sleep();
#endif

    while (m_running) {

#ifdef DEBUG_FPS
        i++;
#endif

#ifndef SLEEP_MS_MOTION
        auto motion_start = std::chrono::high_resolution_clock::now();
#endif

        m_motion_detected = false;
        if (m_enable_motion) {
            cv::Mat frame0_get = m_readers[0]->get_latest_frame();
            if (!frame0_get.empty() && frame0_get.size() == cv::Size(W_0, H_0)) {
                m_frame0 = frame0_get(cv::Rect(0, 0, CROP_WIDTH, CROP_HEIGHT));
                detect_largest_motion_area_set_channel();
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

#ifndef SLEEP_MS_MOTION
        // Calculate sleep time based on measured FPS
        double fps = m_readers[0]->get_fps();
        double frame_time = (fps > 0.0) ? (1.0 / fps) : 1.0 / 20.0; // Default to 20 FPS if zero
        auto detect_time = std::chrono::high_resolution_clock::now() - motion_start;

        auto sleep_time = std::chrono::duration<double>(frame_time) - detect_time;
        m_motion_sleep_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time).count();
#endif

#ifdef DEBUG_FPS
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
#endif

    try {

#ifdef DEBUG_FPS
        int i = 0;
#endif

        while (m_running) {

#ifdef SLEEP_MS_DRAW_DETECTED
            m_draw_sleep_ms = SLEEP_MS_DRAW;
#endif

#ifdef DEBUG_FPS
            i++;
#endif

#ifndef SLEEP_MS_DRAW
            auto draw_start = std::chrono::high_resolution_clock::now();
#endif

            if (m_enable_tour) { do_tour_logic(); }

            cv::Mat get;
            if (m_enable_minimap_fullscreen) {
                get = m_frame0_dbuff.get();
            }
            else if (m_enable_fullscreen_channel ||
                     (m_display_mode == DISPLAY_MODE_SINGLE) ||
                     (m_enable_motion && m_enable_motion_zoom_largest && (m_motion_detected_min_ms || m_motion_detect_linger))) {
                get = m_readers[m_current_channel]->get_latest_frame();
                draw_motion_region(get, 0, 0, get.size().width, get.size().height);
            }
            else if (m_display_mode == DISPLAY_MODE_SORT) {
                get = paint_main_mat_sort();
            }
            else if (m_display_mode == DISPLAY_MODE_KING) {
                get = paint_main_mat_king();
            }
            else if (m_display_mode == DISPLAY_MODE_TOP) {
                get = paint_main_mat_top();
            }
            else if (m_display_mode == DISPLAY_MODE_ALL) {
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

            if (m_enable_minimap) { draw_minimap(); }
            if (m_enable_info) { draw_info(); }
            if (m_enable_info_line) { draw_info_line(); }

            cv::imshow("Motion", m_main_display);

            handle_keys();

#ifndef SLEEP_MS_DRAW
            // Calculate sleep time based on measured FPS
            double fps = m_readers[m_current_channel]->get_fps();
            double frame_time = (fps > 0.0) ? (1.0 / fps) : 1.0 / 30.0; // Default to 30 FPS if zero
            auto draw_time = std::chrono::high_resolution_clock::now() - draw_start;

            auto sleep_time = std::chrono::duration<double>(frame_time) - draw_time;
            m_draw_sleep_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time).count();
#endif

#ifdef DEBUG_FPS
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
