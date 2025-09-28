#include "motion_detector.hpp"
#include "utils.hpp"

void MotionDetector::draw_loop_handle_keys()
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
