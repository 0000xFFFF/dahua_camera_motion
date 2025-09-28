#include "motion_detector.hpp"
#include "utils.hpp"

void MotionDetector::draw_paint_info()
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

void MotionDetector::draw_paint_info_line()
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
