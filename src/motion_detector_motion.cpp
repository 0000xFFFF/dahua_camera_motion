#include "debug.hpp"
#include "motion_detector.hpp"
#include "utils.hpp"
#include <SDL2/SDL_mixer.h>

extern Mix_Chunk* g_sfx_8bit_clicky;

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

    m_frame0_dbuff.update(m_frame0);
}
