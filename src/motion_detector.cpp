#include "motion_detector.hpp"
#include "debug.hpp"
#include "globals.hpp"
#include "opencv2/highgui.hpp"
#include "utils.hpp"
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <opencv2/bgsegm.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

extern Mix_Chunk* g_sfx_8bit_clicky;

cv::Mat MotionDetector::get_frame(int channel, int layout_changed)
{

    if (m_low_cpu) {
        constexpr int mini_ch_w = W_0 / 3;
        constexpr int mini_ch_h = H_0 / 3;

        cv::Mat frame0 = m_frame0;
        cv::Mat frame;

        if (frame0.empty()) { return frame; }

        int row = (channel - 1) / 3; // groups of 3 channels
        if (channel >= 7) row = 2;   // adjust since only 2 channels in last row
        int col = (channel - 1) % 3; // column index
        frame = frame0(cv::Rect(mini_ch_w * col, mini_ch_h * row, mini_ch_w, mini_ch_h));

        return frame;
    }

    return m_readers[channel]->get_latest_frame(layout_changed);
}

std::tuple<long, long, long, long> MotionDetector::parse_area(const std::string& input)
{
    size_t posSemicolon = input.find(';');
    std::string first = input.substr(0, posSemicolon);
    std::string second = input.substr(posSemicolon + 1);

    size_t posX1 = first.find('x');
    size_t posX2 = second.find('x');

    long x = std::stol(first.substr(0, posX1));
    long y = std::stol(first.substr(posX1 + 1));
    long w = std::stol(second.substr(0, posX2));
    long h = std::stol(second.substr(posX2 + 1));

    return std::make_tuple(x, y, w, h);
}

// e.g. "100x200,150x250,...;300x400,350x450,...";
void MotionDetector::parse_ignore_contours(const std::string& input)
{
    std::vector<std::vector<cv::Point>> contours;
    std::stringstream ss(input);
    std::string contourStr;

    while (std::getline(ss, contourStr, ';')) { // Split contours by ";"
        std::vector<cv::Point> contour;
        std::stringstream pointStream(contourStr);
        std::string pointStr;

        while (std::getline(pointStream, pointStr, ',')) { // Split points by ","
            size_t xPos = pointStr.find('x');              // Find 'x' separator

            if (xPos != std::string::npos) {
                int x = std::stoi(pointStr.substr(0, xPos));
                int y = std::stoi(pointStr.substr(xPos + 1));
                contour.push_back(cv::Point(x, y));
            }
        }

        if (!contour.empty()) {
            contours.push_back(contour);
        }
    }

    m_ignore_contours.update(contours);
}

void MotionDetector::parse_ignore_contours_file(const std::string& filename)
{
    std::vector<std::vector<cv::Point>> contours;
    std::ifstream file(filename);
    std::string contourStr;

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    while (std::getline(file, contourStr)) { // Read contours line by line
        std::vector<cv::Point> contour;
        std::stringstream pointStream(contourStr);
        std::string pointStr;

        while (std::getline(pointStream, pointStr, ',')) { // Split points by ","
            size_t xPos = pointStr.find('x');              // Find 'x' separator

            if (xPos != std::string::npos) {
                int x = std::stoi(pointStr.substr(0, xPos));
                int y = std::stoi(pointStr.substr(xPos + 1));
                contour.push_back(cv::Point(x, y));
            }
        }

        if (!contour.empty()) {
            contours.push_back(contour);
        }
    }

    file.close();
    m_ignore_contours.update(contours);
}

void print_contour(const std::vector<cv::Point>& contour)
{
    for (size_t x = 0; x < contour.size(); x++) {
        std::cout << contour[x].x << "x" << contour[x].y;
        if (x != contour.size() - 1) { std::cout << ","; }
    }
}

void MotionDetector::print_ignore_contours()
{
    auto ignore_contours = m_ignore_contours.get();
    std::cout << "-ic \"";
    for (size_t i = 0; i < ignore_contours.size(); i++) {
        for (size_t x = 0; x < ignore_contours[i].size(); x++) {
            std::cout << ignore_contours[i][x].x << "x" << ignore_contours[i][x].y;
            if (x != ignore_contours[i].size() - 1) { std::cout << ","; }
        }
        if (i != ignore_contours.size() - 1) { std::cout << ";"; }
    }
    std::cout << "\"" << std::endl;
}

void MotionDetector::parse_alarm_pixels(const std::string& input)
{
    std::vector<cv::Point> pixels;
    std::stringstream ss(input);
    std::string pointStr;

    while (std::getline(ss, pointStr, ';')) {
        size_t xPos = pointStr.find('x');

        if (xPos != std::string::npos) {
            int x = std::stoi(pointStr.substr(0, xPos));
            int y = std::stoi(pointStr.substr(xPos + 1));
            pixels.push_back(cv::Point(x, y));
        }
    }

    m_alarm_pixels.update(pixels);
}

void MotionDetector::parse_alarm_pixels_file(const std::string& filename)
{
    std::vector<cv::Point> pixels;
    std::ifstream file(filename);
    std::string pointStr;

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    while (std::getline(file, pointStr)) {
        size_t xPos = pointStr.find('x');
        if (xPos != std::string::npos) {
            int x = std::stoi(pointStr.substr(0, xPos));
            int y = std::stoi(pointStr.substr(xPos + 1));
            pixels.push_back(cv::Point(x, y));
        }
    }

    file.close();

    m_alarm_pixels.update(pixels);
}

void MotionDetector::print_alarm_pixels()
{
    auto aps = m_alarm_pixels.get();
    std::cout << "-ap \"";
    for (size_t i = 0; i < aps.size(); i++) {
        std::cout << aps[i].x << "x" << aps[i].y;
        if (i != aps.size() - 1) { std::cout << ","; }
    }
    std::cout << "\"" << std::endl;
}

MotionDetector::MotionDetector(const std::string& ip,
                               const std::string& username,
                               const std::string& password,
                               int width,
                               int height,
                               bool fullscreen,
                               int display_mode,
                               int area,
                               int rarea,
                               int motion_detect_min_ms,
                               int current_channel,
                               int enable_motion,
                               int enable_motion_zoom_largest,
                               int enable_tour,
                               int tour_ms,
                               int enable_info,
                               int enable_info_line,
                               int enable_info_rect,
                               int enable_minimap,
                               int enable_minimap_fullscreen,
                               int enable_fullscreen_channel,
                               int enable_ignore_contours,
                               int enable_alarm_pixels,
                               const std::string& ignore_contours,
                               const std::string& ignore_contours_file,
                               const std::string& alarm_pixels,
                               const std::string& alarm_pixels_file,
                               int focus_channel,
                               const std::string& focus_channel_area,
                               int focus_channel_sound,
                               int low_cpu)
    :

      m_display_width(width),
      m_display_height(height),
      m_fullscreen(fullscreen),
      m_display_mode(display_mode),
      m_motion_min_area(area),
      m_motion_min_rect_area(rarea),
      m_motion_detect_min_ms(motion_detect_min_ms),
      m_tour_ms(tour_ms),
      m_low_cpu(low_cpu),
      m_current_channel(current_channel),
      m_enable_motion(enable_motion),
      m_enable_motion_zoom_largest(enable_motion_zoom_largest),
      m_enable_tour(enable_tour),
      m_enable_info(enable_info),
      m_enable_info_line(enable_info_line),
      m_enable_info_rect(enable_info_rect),
      m_enable_minimap(enable_minimap),
      m_enable_minimap_fullscreen(enable_minimap_fullscreen),
      m_enable_fullscreen_channel(enable_fullscreen_channel),
      m_enable_ignore_contours(enable_ignore_contours),
      m_enable_alarm_pixels(enable_alarm_pixels),
      m_focus_channel(focus_channel),
      m_focus_channel_sound(focus_channel_sound),
      m_canv1(cv::Mat(cv::Size(width, height), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_canv2(cv::Mat(cv::Size(width, height), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_main_display(cv::Mat(cv::Size(width, height), CV_8UC3, cv::Scalar(0, 0, 0)))

{
    if (!ignore_contours.empty()) parse_ignore_contours(ignore_contours);
    if (!ignore_contours_file.empty()) parse_ignore_contours_file(ignore_contours_file);
    print_ignore_contours();

    if (!alarm_pixels.empty()) parse_alarm_pixels(alarm_pixels);
    if (!alarm_pixels_file.empty()) parse_alarm_pixels_file(alarm_pixels_file);
    print_alarm_pixels();

    // m_fgbg = cv::createBackgroundSubtractorMOG2(20, 32, true);
    m_fgbg = cv::createBackgroundSubtractorKNN(20, 400.0, true);
    // m_fgbg = cv::bgsegm::createBackgroundSubtractorCNT(true, 15, true);
    //
    if (low_cpu) {
        m_readers.emplace_back(std::make_unique<FrameReader>(0, ip, username, password, true));
        for (int channel = 1; channel <= CHANNEL_COUNT; ++channel) {
            m_readers.emplace_back(std::make_unique<FrameReader>(channel, ip, username, password, false));
        }

        change_channel(current_channel);
    }
    else if (focus_channel == -1) {

        m_readers.emplace_back(std::make_unique<FrameReader>(0, ip, username, password, true));
        for (int channel = 1; channel <= CHANNEL_COUNT; ++channel) {
            m_readers.emplace_back(std::make_unique<FrameReader>(channel, ip, username, password, true));
        }

        change_channel(current_channel);
    }
    else {

        for (int channel = 0; channel <= CHANNEL_COUNT; channel++) {
            m_readers.emplace_back(std::make_unique<FrameReader>(channel, ip, username, password, channel == focus_channel));
        }

        if (!focus_channel_area.empty() && focus_channel_area != "") {
            auto [x, y, w, h] = parse_area(focus_channel_area);
            m_focus_channel_area_x = x;
            m_focus_channel_area_y = y;
            m_focus_channel_area_w = w;
            m_focus_channel_area_h = h;
            m_focus_channel_area_set = true;
            D(std::cout << "focus channel area: "
                        << m_focus_channel_area_x << "x"
                        << m_focus_channel_area_y << ";"
                        << m_focus_channel_area_w << "x"
                        << m_focus_channel_area_h << std::endl;);
        }
    }

    m_thread_detect_motion = std::thread([this]() { detect_motion(); });

    D(std::cout << "m_display_width: " << m_display_width << std::endl);
    D(std::cout << "m_display_height: " << m_display_height << std::endl);
    D(std::cout << "m_fullscreen: " << m_fullscreen << std::endl);
    D(std::cout << "m_display_mode: " << m_display_mode.load() << std::endl);
    D(std::cout << "m_motion_min_area: " << m_motion_min_area << std::endl);
    D(std::cout << "m_motion_min_rect_area: " << m_motion_min_rect_area << std::endl);
    D(std::cout << "m_motion_detect_min_ms: " << m_motion_detect_min_ms << std::endl);
    D(std::cout << "m_current_channel: " << m_current_channel.load() << std::endl);
    D(std::cout << "m_enable_motion: " << m_enable_motion.load() << std::endl);
    D(std::cout << "m_enable_motion_zoom_largest: " << m_enable_motion_zoom_largest.load() << std::endl);
    D(std::cout << "m_enable_tour: " << m_enable_tour.load() << std::endl);
    D(std::cout << "m_enable_info: " << m_enable_info.load() << std::endl);
    D(std::cout << "m_enable_info_line: " << m_enable_info_line.load() << std::endl);
    D(std::cout << "m_enable_info_rect: " << m_enable_info_rect.load() << std::endl);
    D(std::cout << "m_enable_minimap: " << m_enable_minimap.load() << std::endl);
    D(std::cout << "m_enable_minimap_fullscreen: " << m_enable_minimap_fullscreen.load() << std::endl);
    D(std::cout << "m_enable_fullscreen_channel: " << m_enable_fullscreen_channel.load() << std::endl);
    D(std::cout << "m_enable_ignore_contours: " << m_enable_ignore_contours.load() << std::endl);
    D(std::cout << "m_enable_alarm_pixels: " << m_enable_alarm_pixels.load() << std::endl);
    D(std::cout << "m_focus_channel: " << m_focus_channel.load() << std::endl);
    D(std::cout << "m_focus_channel_area_set: " << m_focus_channel_area_set.load() << std::endl);
    D(std::cout << "m_focus_channel_area_x: " << m_focus_channel_area_x.load() << std::endl);
    D(std::cout << "m_focus_channel_area_y: " << m_focus_channel_area_y.load() << std::endl);
    D(std::cout << "m_focus_channel_area_w: " << m_focus_channel_area_w.load() << std::endl);
    D(std::cout << "m_focus_channel_area_h: " << m_focus_channel_area_h.load() << std::endl);
    D(std::cout << "m_focus_channel_sound: " << m_focus_channel_sound.load() << std::endl);
    D(std::cout << "m_low_cpu: " << m_low_cpu << std::endl);
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
    m_layout_changed = true;
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

    if (elapsed >= m_tour_ms) {
        m_tour_start = std::chrono::high_resolution_clock::now();
#if KING_LAYOUT == KING_LAYOUT_CIRC
        change_channel(m_king_chain.get().back());
#else

#if KING_LAYOUT == KING_LAYOUT_REL

        m_tour_current_channel++;
        if (m_tour_current_channel > CHANNEL_COUNT) { m_tour_current_channel = 1; }

        int ch = m_tour_current_channel;
        auto vec = m_king_chain.get();
        std::vector<int> n(vec.size());
        int x = 0;
        n[x++] = ch;
        for (int i = 0; i < CHANNEL_COUNT; i++) {
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
        if (m_tour_current_channel > CHANNEL_COUNT) { m_tour_current_channel = 1; }
        change_channel(m_tour_current_channel);
#endif
#endif
    }
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
    // ignore area by blacking it out
    if (m_enable_ignore_contours) {
        auto ics = m_ignore_contours.get();
        auto ic = m_ignore_contour.get();
        std::vector<std::vector<cv::Point>> outline = {ic};
        cv::polylines(m_frame0, outline, false, cv::Scalar(0));

        if (!ics.empty()) {
            // erase empty
            for (size_t i = 0; i < ics.size(); ++i) {
                if (ics[i].empty()) {
                    ics.erase(ics.begin() + i);
                    --i;
                }
            }

            // blackout the ignored area
            cv::fillPoly(m_frame0, ics, cv::Scalar(0));
        }
    }

    // finding motion contours
    cv::Mat fgmask;
    m_fgbg->apply(m_frame0, fgmask);

    cv::Mat thresh;
    cv::threshold(fgmask, thresh, 128, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Find largest motion area
    std::vector<cv::Point> max_contour;
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
                    max_contour = contour;
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
        m_motion_detected_min_ms = motion_duration >= m_motion_detect_min_ms;
        if (m_motion_detected_min_ms) {
            if (m_focus_channel == -1) {

                float rel_x = motion_region.x / static_cast<float>(W_0);
                float rel_y = motion_region.y / static_cast<float>(H_0);

                int col = static_cast<int>(rel_x * 3);
                int row = static_cast<int>(rel_y * 3);

                int new_channel;
                if (row == 0) { new_channel = 1 + col; } // first row
                else if (row == 1) {
                    new_channel = 4 + col;
                } // second row
                else {
                    new_channel = (col == 0 ? 7 : 8);
                } // third row

                if (m_current_channel != new_channel) {
                    change_channel(new_channel);
                }
            }
            m_motion_detect_linger_start = now;
            m_motion_detect_linger = true;
        }

        if (m_motion_detected_min_ms && m_focus_channel != -1 && m_focus_channel_sound) {
            play_unique_sound(g_sfx_8bit_clicky); // play sfx alarm if in detected area
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

    constexpr int mini_ch_w = W_0 / 3;
    constexpr int mini_ch_h = H_0 / 3;

    // drawing alarm pixels
    if (m_enable_alarm_pixels && motion_region.size().width < mini_ch_w && motion_region.size().height < mini_ch_h) {
        auto ap = m_alarm_pixels.get();
        if (!ap.empty()) {
            for (size_t i = 0; i < ap.size(); i++) {
                cv::Point p = ap[i];
                m_frame0.at<cv::Vec3b>(p.y, p.x) = cv::Vec3b(0, 0, 255); // BGR

                if (!max_contour.empty()) {
                    if (cv::pointPolygonTest(max_contour, cv::Point2f(p.x, p.y), false) >= 0) {
                        play_unique_sound(g_sfx_8bit_clicky); // play sfx alarm if in detected area
                    }
                }
            }
        }
    }

    if (m_enable_minimap || m_enable_minimap_fullscreen || m_focus_channel != -1)
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

    cv::putText(m_main_display, "Info (i/PAGE UP): " + bool_to_str(m_enable_info),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Display Mode (up/down,n,a,s,k,j): " + std::to_string(m_display_mode),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Motion (m/* ; l//): " + bool_to_str(m_enable_motion) + " ; " + bool_to_str(m_enable_motion_zoom_largest),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Minimap (o/PAGE DOWN ; 0): " + bool_to_str(m_enable_minimap) + " ; " + bool_to_str(m_enable_minimap_fullscreen),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Motion Detected: " + std::to_string(m_motion_detected) + " " + std::to_string(m_motion_detected_min_ms),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Channel (num): " + std::to_string(m_current_channel),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Fullscreen (f/+): " + bool_to_str(m_enable_fullscreen_channel),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Tour (t/.): " + bool_to_str(m_enable_tour),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Alarm (d/ENTER): " + bool_to_str(m_enable_alarm_pixels),
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_display, "Reset (r/BACKSPACE)",
                cv::Point(10, text_y_start + i++ * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
}

void MotionDetector::draw_info_line()
{
    if (m_motion_detected_min_ms) {
        cv::rectangle(m_main_display, cv::Rect(0, 0, m_main_display.size().width, m_main_display.size().height), cv::Scalar(0, 0, 255), 1, cv::LINE_8);
    }
    else if (m_motion_detect_linger) {
        cv::rectangle(m_main_display, cv::Rect(0, 0, m_main_display.size().width, m_main_display.size().height), cv::Scalar(0, 255, 255), 1, cv::LINE_8);
    }
    else if (m_enable_tour) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_tour_start).count();
        float p = (float)elapsed / m_tour_ms;
        if (p > 100) { p = 100; }
        cv::rectangle(m_main_display, cv::Rect(0, 0, m_main_display.size().width, m_main_display.size().height), cv::Scalar(0, 0, 0), 1, cv::LINE_8);

        int w = m_main_display.size().width - 1;
        int h = m_main_display.size().height - 1;
        cv::line(m_main_display, cv::Point(0, 0), cv::Point(p * w, 0), cv::Scalar(0, 165, 255), 1, cv::LINE_8);     // top
        cv::line(m_main_display, cv::Point(w, 0), cv::Point(w, p * h), cv::Scalar(0, 165, 255), 1, cv::LINE_8);     // right
        cv::line(m_main_display, cv::Point(w, h), cv::Point(w - p * w, h), cv::Scalar(0, 165, 255), 1, cv::LINE_8); // bottom
        cv::line(m_main_display, cv::Point(0, h), cv::Point(0, h - p * h), cv::Scalar(0, 165, 255), 1, cv::LINE_8); // left
    }
    else {
        cv::rectangle(m_main_display, cv::Rect(0, 0, m_main_display.size().width, m_main_display.size().height), cv::Scalar(0, 0, 0), 1, cv::LINE_8);
    }
}

cv::Mat MotionDetector::paint_main_mat_all()
{
    bool layout_changed = m_layout_changed;
    size_t w = m_display_width / 3;
    size_t h = m_display_height / 3;

    cv::parallel_for_(cv::Range(0, CHANNEL_COUNT), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            int ch = i + 1;
            cv::Mat mat = get_frame(ch, layout_changed);
            if (mat.empty()) { continue; }

            int row = i / 3;
            int col = i % 3;
            int x = col * w;
            int y = row * h;
            cv::resize(mat, m_canv2(cv::Rect(x, y, w, h)), cv::Size(w, h));
            if (ch == m_current_channel) {
                draw_motion_region(m_canv2, x, y, w, h);
            }
        }
    });

    m_layout_changed = false;
    return m_canv2;
}

cv::Mat MotionDetector::paint_main_mat_sort()
{
    bool layout_changed = m_layout_changed;

    size_t w = m_display_width / 3;
    size_t h = m_display_height / 3;

    std::vector vec = m_king_chain.get();

    cv::parallel_for_(cv::Range(0, CHANNEL_COUNT), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            int ch = vec[i];
            cv::Mat mat = get_frame(ch, layout_changed);
            if (mat.empty()) { continue; }

            int row = i / 3;
            int col = i % 3;
            int x = col * w;
            int y = row * h;
            cv::Rect roi(x, y, w, h);
            cv::resize(mat, m_canv2(cv::Rect(x, y, w, h)), cv::Size(w, h));
            if (ch == m_current_channel) {
                draw_motion_region(m_canv2, x, y, w, h);
            }
        }
    });

    m_layout_changed = false;
    return m_canv2;
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

        constexpr int mini_ch_w = W_0 / 3;
        constexpr int mini_ch_h = H_0 / 3;

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
    bool layout_changed = m_layout_changed;

    size_t w = m_display_width / 4;
    size_t h = m_display_height / 4;

    std::vector vec = m_king_chain.get();

    cv::parallel_for_(cv::Range(0, CHANNEL_COUNT), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            cv::Mat mat = get_frame(vec[i], layout_changed);
            if (mat.empty()) { continue; }

#if KING_LAYOUT == KING_LAYOUT_REL
            switch (i) {
                case 0:
                    {
                        size_t w0 = w * 3;
                        size_t h0 = h * 3;
                        cv::resize(mat, m_canv1(cv::Rect(0 * w, 0 * h, w0, h0)), cv::Size(w0, h0));
                        draw_motion_region(m_canv1(cv::Rect(0 * w, 0 * h, w0, h0)), 0, 0, w0, h0);
                        break;
                    }
                case 1: cv::resize(mat, m_canv1(cv::Rect(3 * w, 0 * h, w, h)), cv::Size(w, h)); break;
                case 2: cv::resize(mat, m_canv1(cv::Rect(3 * w, 1 * h, w, h)), cv::Size(w, h)); break;
                case 3: cv::resize(mat, m_canv1(cv::Rect(3 * w, 2 * h, w, h)), cv::Size(w, h)); break;

                case 4: cv::resize(mat, m_canv1(cv::Rect(0 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 5: cv::resize(mat, m_canv1(cv::Rect(1 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 6: cv::resize(mat, m_canv1(cv::Rect(2 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 7: cv::resize(mat, m_canv1(cv::Rect(3 * w, 3 * h, w, h)), cv::Size(w, h)); break;
            }
#endif

#if KING_LAYOUT == KING_LAYOUT_CIRC
            // CIRCLE LAYOUR
            switch (i) {
                case 0:
                    {
                        size_t w0 = w * 3;
                        size_t h0 = h * 3;
                        cv::resize(mat, m_canv1(cv::Rect(0 * w, 0 * h, w0, h0)), cv::Size(w0, h0));
                        draw_motion_region(m_canv1, 0, 0, w0, h0);
                        break;
                    }
                case 1: cv::resize(mat, m_canv1(cv::Rect(3 * w, 0 * h, w, h)), cv::Size(w, h)); break;
                case 2: cv::resize(mat, m_canv1(cv::Rect(3 * w, 1 * h, w, h)), cv::Size(w, h)); break;
                case 3: cv::resize(mat, m_canv1(cv::Rect(3 * w, 2 * h, w, h)), cv::Size(w, h)); break;

                case 7: cv::resize(mat, m_canv1(cv::Rect(0 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 6: cv::resize(mat, m_canv1(cv::Rect(1 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 5: cv::resize(mat, m_canv1(cv::Rect(2 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 4: cv::resize(mat, m_canv1(cv::Rect(3 * w, 3 * h, w, h)), cv::Size(w, h)); break;
            }
#endif
        }
    });

    m_layout_changed = false;
    return m_canv1;
}

cv::Mat MotionDetector::paint_main_mat_top()
{
    bool layout_changed = m_layout_changed;

    size_t w = m_display_width / 4;
    size_t h = m_display_height / 4;

    std::vector<int> active_channels;
    for (int i = 1; i <= CHANNEL_COUNT; i++) {
        if (i != m_current_channel) {
            active_channels.push_back(i);
        }
    }

    cv::parallel_for_(cv::Range(0, CHANNEL_COUNT), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            cv::Mat mat;

            if (i == 0) {
                mat = get_frame(m_current_channel, layout_changed);
                if (mat.empty()) { continue; }

                size_t w0 = w * 3;
                size_t h0 = h * 3;
                cv::resize(mat, m_canv1(cv::Rect(0 * w, 0 * h, w0, h0)), cv::Size(w0, h0));
                draw_motion_region(m_canv1, 0, 0, w0, h0);
            }
            else {
                // Other slots are from active_channels (excluding m_current_channel)
                int channel = active_channels[i - 1]; // Skip 0th slot
                mat = get_frame(channel, layout_changed);
                if (mat.empty()) { continue; }

                // Layout for circular arrangement
                switch (i) {
                    case 1: cv::resize(mat, m_canv1(cv::Rect(3 * w, 0 * h, w, h)), cv::Size(w, h)); break;
                    case 2: cv::resize(mat, m_canv1(cv::Rect(3 * w, 1 * h, w, h)), cv::Size(w, h)); break;
                    case 3: cv::resize(mat, m_canv1(cv::Rect(3 * w, 2 * h, w, h)), cv::Size(w, h)); break;
                    case 7: cv::resize(mat, m_canv1(cv::Rect(0 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                    case 6: cv::resize(mat, m_canv1(cv::Rect(1 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                    case 5: cv::resize(mat, m_canv1(cv::Rect(2 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                    case 4: cv::resize(mat, m_canv1(cv::Rect(3 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                }
            }
        }
    });

    m_layout_changed = false;
    return m_canv1;
}

void MotionDetector::handle_keys()
{
    // clang-format off
    int key = cv::waitKey(1);
    if (key == 'q') { stop(); }
    else if (key == 'm' || key == '*') {
        m_enable_motion = !m_enable_motion;
        if (!m_enable_motion) {
            m_motion_detected = false;
            m_motion_detected_min_ms = false;
            m_motion_detect_linger = false;
        }
    }
    else if (key == 'l' || key == '/') { m_enable_motion_zoom_largest = !m_enable_motion_zoom_largest; }
    else if (key == 'n') { m_display_mode = DISPLAY_MODE_SINGLE; }
    else if (key == 'a') { m_display_mode = DISPLAY_MODE_ALL; }
    else if (key == 's') { m_display_mode = DISPLAY_MODE_SORT; }
    else if (key == 'j') { m_display_mode = DISPLAY_MODE_TOP; }
    else if (key == 'k') { m_display_mode = DISPLAY_MODE_KING; }
    else if (key == KEY_LINUX_ARROW_UP || key == KEY_WIN_ARROW_UP) { m_display_mode++; if (m_display_mode > 4) { m_display_mode = 4; } }
    else if (key == KEY_LINUX_ARROW_DOWN || key == KEY_WIN_ARROW_DOWN) { m_display_mode--; if (m_display_mode < 0) { m_display_mode = 0; } }
    else if (key == 'i' || key == KEY_LINUX_PAGE_UP || key == KEY_WIN_PAGE_UP) { m_enable_info = !m_enable_info; }
    else if (key == 'o' || key == KEY_LINUX_PAGE_DOWN || key == KEY_LINUX_PAGE_DOWN) { m_enable_minimap = !m_enable_minimap; }
    else if (key == 'f' || key == '+') { m_enable_fullscreen_channel = !m_enable_fullscreen_channel; }
    else if (key == 't' || key == '.') { m_enable_tour = !m_enable_tour; }
    else if (key == 'r' || key == KEY_BACKSPACE) {
        m_current_channel = 1;
        m_king_chain.update({1, 2, 3, 4, 5, 6, 7, 8});
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
    else if (key == KEY_LINUX_ARROW_LEFT || key == KEY_WIN_ARROW_LEFT) {
        int new_ch = m_current_channel + 1;
        if (new_ch > CHANNEL_COUNT) new_ch = 1;
        change_channel(new_ch);
    } else if (key == KEY_LINUX_ARROW_RIGHT || key == KEY_WIN_ARROW_LEFT) { 
        int new_ch = m_current_channel - 1;
        if (new_ch < 1) new_ch = CHANNEL_COUNT;
        change_channel(new_ch);
    }
    else if (key >= '1' && key <= '0' + CHANNEL_COUNT) {
        change_channel(key - '0');
    }
    else if (key == 'c') {
        auto ic = m_ignore_contour.get();
        auto mp = m_mouse_pos.get();
        if (mp != cv::Point()) {
            ic.push_back(mp);
            m_ignore_contour.update(ic);
        }
    }
    else if (key == 'v') {
        auto ic = m_ignore_contour.get();
        std::cout << "contour: ";
        print_contour(ic);
        std::cout << std::endl;
        if (!ic.empty()) {
            auto ics = m_ignore_contours.get();
            ics.push_back(ic);
            m_ignore_contours.update(ics);
            m_ignore_contour.update({});
            print_ignore_contours();
        }
    }
    else if (key == 'b') {
        std::cout << "cleared all ignore area/contours" << std::endl;
        m_ignore_contours.update({});
        m_ignore_contour.update({});
    }
    else if (key == 'd' || key == KEY_ENTER) {
        m_enable_alarm_pixels = !m_enable_alarm_pixels;
    }
    else if (key == 'z') {
        std::cout << "cleared all alarm pixels" << std::endl;
        m_alarm_pixels.update({});
    }
    else if (key == 'x') {
        auto aps = m_alarm_pixels.get();
        auto mp = m_mouse_pos.get();
        if (mp != cv::Point()) {
            aps.push_back(mp);
            m_alarm_pixels.update(aps);
        }
        print_alarm_pixels();
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

    D(std::cout << "starting motion detection" << std::endl);

    while (m_running) {

#ifdef DEBUG_FPS
        i++;
#endif

#ifndef SLEEP_MS_MOTION
        auto motion_start = std::chrono::high_resolution_clock::now();
#endif

        m_motion_detected = false;
        if (m_enable_motion) {
            if (m_focus_channel == -1) {
                cv::Mat frame0_get = m_readers[0]->get_latest_frame(false);
                if (!frame0_get.empty() && frame0_get.size().width == W_0 && frame0_get.size().height == H_0) {
                    m_frame0 = frame0_get;
                    detect_largest_motion_area_set_channel();
                }
            }
            else {
                cv::Mat frame0_get = m_readers[m_focus_channel]->get_latest_frame(false);

                if (!frame0_get.empty()) {
                    if (m_focus_channel_area_set.load()) { // Check if the area is set
                        // Ensure the coordinates are within the bounds of the frame
                        long x = std::max(0L, m_focus_channel_area_x.load());
                        long y = std::max(0L, m_focus_channel_area_y.load());
                        long w = m_focus_channel_area_w.load();
                        long h = m_focus_channel_area_h.load();

                        if (x + w > frame0_get.cols) w = frame0_get.cols - x;
                        if (y + h > frame0_get.rows) h = frame0_get.rows - y;

                        if (w > 0 && h > 0) {
                            // Crop the subregion
                            cv::Mat roi = frame0_get(cv::Rect(x, y, w, h));
                            cv::resize(roi, m_frame0, cv::Size(m_display_width, m_display_height));
                            detect_largest_motion_area_set_channel();
                        }
                    }
                    else {
                        cv::resize(frame0_get, m_frame0, cv::Size(m_display_width, m_display_height));
                        detect_largest_motion_area_set_channel();
                    }
                }
            }
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

        {
            std::unique_lock<std::mutex> lock(m_mtx_motion);
            m_cv_motion.wait_for(lock, std::chrono::milliseconds(m_motion_sleep_ms), [&] { return !m_running; });
        }
    }

    D(std::cout << "ending motion detection" << std::endl);
}

void on_mouse(int event, int x, int y, int flags, void* userdata)
{
    UNUSED(flags);
    if (event == cv::EVENT_MOUSEMOVE) {
        MotionDetector* detector = static_cast<MotionDetector*>(userdata);
        detector->m_mouse_pos.update(cv::Point(x, y));
    }
    else if (event == cv::EVENT_LBUTTONDOWN) {
        std::cout << "click: " << x << "x" << y << std::endl;
    }
}

void MotionDetector::draw_loop()
{
    cv::namedWindow(DEFAULT_WINDOW_NAME);
    // cv::namedWindow(DEFAULT_WINDOW_NAME, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(DEFAULT_WINDOW_NAME, on_mouse, this);

#ifdef MAKE_IGNORE
    std::cout << "to create ignore area use keys:\n"
              << "   c - create vertex\n"
              << "   v - join contour\n"
              << "   b - clear all" << std::endl;
#endif

    if (m_fullscreen) {
        cv::namedWindow(DEFAULT_WINDOW_NAME, cv::WINDOW_NORMAL);
        cv::setWindowProperty(DEFAULT_WINDOW_NAME, cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
    }

#ifdef SLEEP_MS_DRAW
    m_draw_sleep_ms = SLEEP_MS_DRAW;
#endif

    try {

#ifdef DEBUG_FPS
        int i = 0;
#endif

        while (m_running) {

#ifdef DEBUG_FPS
            i++;
#endif

#ifndef SLEEP_MS_DRAW
            auto draw_start = std::chrono::high_resolution_clock::now();
#endif

            if (m_enable_tour) { do_tour_logic(); }

            cv::Mat get;
            if (m_enable_minimap_fullscreen || m_focus_channel != -1) {
                get = m_frame0_dbuff.get();
            }
            else if (m_enable_fullscreen_channel ||
                     (m_display_mode == DISPLAY_MODE_SINGLE) ||
                     (m_enable_motion && m_enable_motion_zoom_largest && (m_motion_detected_min_ms || m_motion_detect_linger))) {
                get = get_frame(m_current_channel, m_layout_changed);
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
                if (!NO_RESIZE && get.size() != cv::Size(m_display_width, m_display_height)) {
                    cv::resize(get, m_main_display, cv::Size(m_display_width, m_display_height));
                }
                else {
                    m_main_display = get;
                }

                if (m_enable_minimap) { draw_minimap(); }
                if (m_enable_info) { draw_info(); }
                if (m_enable_info_line) { draw_info_line(); }

                cv::imshow(DEFAULT_WINDOW_NAME, m_main_display);

                if (m_display_width == 0) { m_display_width = m_main_display.size().width; }
                if (m_display_height == 0) { m_display_height = m_main_display.size().height; }

                handle_keys();
            }

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

            {
                std::unique_lock<std::mutex> lock(m_mtx_draw);
                m_cv_draw.wait_for(lock, std::chrono::milliseconds(m_draw_sleep_ms), [&] { return !m_running; });
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in display loop: " << e.what() << std::endl;
    }
}

void MotionDetector::stop()
{
    std::cout << "\n\nQuitting..." << std::endl;
    m_running = false;
    m_cv_draw.notify_one();
    m_cv_motion.notify_one();
    if (m_thread_detect_motion.joinable()) { m_thread_detect_motion.join(); }
    for (auto& reader : m_readers) { reader->stop(); }
    D(std::cout << "destroy all win" << std::endl);
    cv::destroyAllWindows();
    D(std::cout << "destroy all win done" << std::endl);
}

std::string MotionDetector::bool_to_str(bool b)
{
    return std::string(b ? "Yes" : "No");
}
