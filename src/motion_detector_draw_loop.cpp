#include "debug.hpp"
#include "motion_detector.hpp"
#include "utils.hpp"

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

    if (m_fullscreen) {
        cv::namedWindow(DEFAULT_WINDOW_NAME, cv::WINDOW_NORMAL);
        cv::setWindowProperty(DEFAULT_WINDOW_NAME, cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
    }

    try {

#ifdef DEBUG_FPS
        int i = 0;
#endif

        std::chrono::time_point<std::chrono::high_resolution_clock> draw_start;

        while (m_running) {

#ifdef DEBUG_FPS
            i++;
#endif
            if (m_sleep_ms_draw_auto) {
                draw_start = std::chrono::high_resolution_clock::now();
            }

            if (m_enable_tour) { do_tour_logic(); }

            cv::UMat get;
            if (m_enable_minimap_fullscreen || m_focus_channel != -1) {
                get = m_frame_detection_dbuff.get();
            }
            else if (m_enable_fullscreen_channel ||
                     (m_display_mode == DISPLAY_MODE_SINGLE) ||
                     (m_enable_motion && m_enable_motion_zoom_largest && (m_motion_detected_min_ms || m_motion_detect_linger))) {

                get = get_frame(m_current_channel, m_layout_changed);
                draw_paint_info_motion_region(get, 0, 0, get.size().width, get.size().height);
            }
            else if (m_display_mode == DISPLAY_MODE_SORT) {
                get = draw_paint_main_mat_sort();
            }
            else if (m_display_mode == DISPLAY_MODE_KING) {
                get = draw_paint_main_mat_king();
            }
            else if (m_display_mode == DISPLAY_MODE_TOP) {
                get = draw_paint_main_mat_top();
            }
            else if (m_display_mode == DISPLAY_MODE_ALL) {
                get = draw_paint_main_mat_all();
            }

            if (!get.empty()) {
                if (!NO_RESIZE && get.size() != cv::Size(m_display_width, m_display_height)) {
                    cv::resize(get, m_main_display, cv::Size(m_display_width, m_display_height));
                }
                else {
                    get.copyTo(m_main_display);
                }

                cv::imshow(DEFAULT_WINDOW_NAME, m_main_display);

                if (m_display_width == 0) { m_display_width = m_main_display.size().width; }
                if (m_display_height == 0) { m_display_height = m_main_display.size().height; }

                draw_loop_handle_keys();
            }

            if (m_sleep_ms_draw_auto) {
                // Calculate sleep time based on measured FPS
                double fps = m_readers[m_current_channel]->get_fps();
                double frame_time = (fps > 0.0) ? (1.0 / fps) : 1.0 / 30.0; // Default to 30 FPS if zero
                auto draw_time = std::chrono::high_resolution_clock::now() - draw_start;

                auto sleep_time = std::chrono::duration<double>(frame_time) - draw_time;
                m_sleep_ms_draw = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time).count();
            }

#ifdef DEBUG_FPS
            if (i % 300 == 0) {
                std::cout << "Draw thread sleep time: " << m_draw_sleep_ms << " ms" << std::endl;
            }
#endif

            {
                std::unique_lock<std::mutex> lock(m_mtx_draw);
                m_cv_draw.wait_for(lock, std::chrono::milliseconds(m_sleep_ms_draw), [&] { return !m_running; });
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in display loop: " << e.what() << std::endl;
    }
}

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
