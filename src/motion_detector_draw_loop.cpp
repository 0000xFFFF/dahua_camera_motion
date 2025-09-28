#include "debug.hpp"
#include "motion_detector.hpp"

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
                draw_paint_motion_region(get, 0, 0, get.size().width, get.size().height);
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
                    m_main_display = get;
                }

                if (m_enable_minimap) { draw_paint_minimap(); }
                if (m_enable_info) { draw_paint_info(); }
                if (m_enable_info_line) { draw_paint_info_line(); }

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
