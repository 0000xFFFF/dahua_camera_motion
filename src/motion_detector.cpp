#include "motion_detector.hpp"
#include "globals.hpp"
#include <opencv2/opencv.hpp>
#include <string>

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
    if (m_tour_frame_index >= m_tour_frame_count) {
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

void MotionDetector::detect_largest_motion_area_set_channel()
{
    m_motion_detected = false;

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
        m_tour_frame_index = 0; // reset so it doesn't auto switch on new tour so we can show a little bit of motion
        float rel_x = m_motion_region.x / static_cast<float>(CROP_WIDTH);
        float rel_y = m_motion_region.y / static_cast<float>(CROP_HEIGHT);
        int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);
        if (new_channel != m_current_channel) { m_current_channel = new_channel; }
    }
}

void MotionDetector::sort_channels_by_motion_area_all_channels()
{
    std::vector<std::vector<cv::Point>> contours = find_contours_frame0();

    m_sorted_chs_area_all = {
        std::make_pair(1, 0.0),
        std::make_pair(2, 0.0),
        std::make_pair(3, 0.0),
        std::make_pair(4, 0.0),
        std::make_pair(5, 0.0),
        std::make_pair(6, 0.0)};

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= m_motion_area) {
            cv::Rect rect = cv::boundingRect(contour);
            cv::rectangle(m_frame0_drawed, rect, cv::Scalar(0, 255, 0), 1);
            double area = rect.width * rect.height;

            float rel_x = rect.x / static_cast<float>(CROP_WIDTH);
            float rel_y = rect.y / static_cast<float>(CROP_HEIGHT);
            int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);

            if (new_channel >= 1 && new_channel <= 6) {
                m_sorted_chs_area_all[new_channel - 1].second = area;
            }
        }
    }

    // higher channel first
    std::sort(m_sorted_chs_area_all.begin(), m_sorted_chs_area_all.end(), [](auto& a, auto& b) { return a.second > b.second; });
}

void MotionDetector::sort_channels_by_motion_area_motion_channels()
{
    std::vector<std::vector<cv::Point>> contours = find_contours_frame0();

    m_sorted_chs_area_motion.clear();

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= m_motion_area) {
            cv::Rect rect = cv::boundingRect(contour);
            cv::rectangle(m_frame0_drawed, rect, cv::Scalar(0, 255, 0), 1);
            double area = rect.width * rect.height;

            float rel_x = rect.x / static_cast<float>(CROP_WIDTH);
            float rel_y = rect.y / static_cast<float>(CROP_HEIGHT);
            int new_channel = 1 + static_cast<int>(rel_x * 3) + (rel_y >= 0.5f ? 3 : 0);

            if (new_channel >= 1 && new_channel <= 6) {
                m_sorted_chs_area_motion.emplace_back(std::make_pair(new_channel, area));
            }
        }
    }

    // higher channel first
    std::sort(m_sorted_chs_area_all.begin(), m_sorted_chs_area_all.end(), [](auto& a, auto& b) { return a.second > b.second; });
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

    cv::putText(m_main_c1r1, "Info (i): " + bool_to_str(m_enableInfo),
                cv::Point(10, text_y_start), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Motion (m/n/l/s/d): " + bool_to_str(m_enableMotion) + "/" + std::to_string(m_motionDisplayMode),
                cv::Point(10, text_y_start + text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Minimap (o): " + bool_to_str(m_enableMinimap),
                cv::Point(10, text_y_start + 2 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Motion Detected: " + std::to_string(m_motion_detected),
                cv::Point(10, text_y_start + 3 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Channel (num): " + std::to_string(m_current_channel),
                cv::Point(10, text_y_start + 4 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Fullscreen (f): " + bool_to_str(m_enableFullscreenChannel),
                cv::Point(10, text_y_start + 5 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Tour (t): " + bool_to_str(m_enableTour) + " " + std::to_string(m_tour_frame_index) + "/" + std::to_string(m_tour_frame_count),
                cv::Point(10, text_y_start + 6 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, font_thickness);
    cv::putText(m_main_c1r1, "Reset (r)",
                cv::Point(10, text_y_start + 7 * text_y_step), cv::FONT_HERSHEY_SIMPLEX,
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
    for (size_t i = 0; i < m_sorted_chs_area_all.size(); i++) {
        cv::Mat mat = m_readers[m_sorted_chs_area_all[i].first]->get_latest_frame();

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

cv::Mat MotionDetector::paint_main_mat_pyramid()
{
    switch (m_sorted_chs_area_motion.size()) {
    case 1:
        {
            cv::Mat mat = m_readers[m_sorted_chs_area_motion[0].first]->get_latest_frame();
            if (mat.empty()) { return cv::Mat(); }
            return mat;
            break;
        }

    case 2:
        {
            cv::Mat output_c1r2 = cv::Mat(cv::Size(W_HD * 1, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0));
            cv::Mat t = m_readers[m_sorted_chs_area_motion[0].first]->get_latest_frame();
            if (!t.empty()) { t.copyTo(output_c1r2(cv::Rect(0 * W_HD, 0 * H_HD, W_HD, H_HD))); }
            cv::Mat b = m_readers[m_sorted_chs_area_motion[1].first]->get_latest_frame();
            if (!b.empty()) { b.copyTo(output_c1r2(cv::Rect(0 * W_HD, 1 * H_HD, W_HD, H_HD))); }
            return output_c1r2;
            break;
        }

    case 3:
        {

            break;
        }
    case 4:
        {
            cv::Mat output_c2r2 = cv::Mat(cv::Size(W_HD * 2, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0));
            cv::Mat tr = m_readers[m_sorted_chs_area_motion[0].first]->get_latest_frame();
            if (!tr.empty()) { tr.copyTo(output_c2r2(cv::Rect(0 * W_HD, 0 * H_HD, W_HD, H_HD))); }
            cv::Mat tl = m_readers[m_sorted_chs_area_motion[1].first]->get_latest_frame();
            if (!tl.empty()) { tl.copyTo(output_c2r2(cv::Rect(0 * W_HD, 0 * H_HD, W_HD, H_HD))); }
            cv::Mat br = m_readers[m_sorted_chs_area_motion[2].first]->get_latest_frame();
            if (!br.empty()) { br.copyTo(output_c2r2(cv::Rect(0 * W_HD, 0 * H_HD, W_HD, H_HD))); }
            cv::Mat bl = m_readers[m_sorted_chs_area_motion[3].first]->get_latest_frame();
            if (!bl.empty()) { bl.copyTo(output_c2r2(cv::Rect(0 * W_HD, 0 * H_HD, W_HD, H_HD))); }
            return output_c2r2;
            break;
        }
    case 5:
        {

            break;
        }
    case 6:
        {
            for (size_t i = 0; i < 6; i++) {
                cv::Mat mat = m_readers[m_sorted_chs_area_motion[i].first]->get_latest_frame();
                if (mat.empty()) { continue; }
                int row = i / 3;
                int col = i % 3;
                cv::Rect roi(col * W_HD, row * H_HD, W_HD, H_HD);
                mat.copyTo(m_main_c3r2(roi));
            }

            return m_main_c3r2;
            break;
        }
    }

    return cv::Mat();
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
            if (m_enableMotion && m_motionDisplayMode == MOTION_DISPLAY_MODE_LARGEST_ONLY) { detect_largest_motion_area_set_channel(); }
            if (m_enableMotion && m_motionDisplayMode == MOTION_DISPLAY_MODE_SORT_BY_AREA_ALL) { sort_channels_by_motion_area_all_channels(); }
            if (m_enableMotion && m_motionDisplayMode == MOTION_DISPLAY_MODE_SORT_BY_AREA_MOTION) { sort_channels_by_motion_area_motion_channels(); }

            if (m_current_channel == 0) {
                main_frame_get = m_frame0_drawed;
            }
            else if (m_enableFullscreenChannel || m_enableTour || m_motion_detected) {
                main_frame_get = m_readers[m_current_channel]->get_latest_frame();
            }
            else if (m_motionDisplayMode == MOTION_DISPLAY_MODE_SORT_BY_AREA_ALL) {
                main_frame_get = paint_main_mat_sort();
            }
            else if (m_motionDisplayMode == MOTION_DISPLAY_MODE_SORT_BY_AREA_MOTION) {
                main_frame_get = paint_main_mat_pyramid();
            }
            else {
                main_frame_get = paint_main_mat_all();
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
            else if (key == 'l') { m_motionDisplayMode = MOTION_DISPLAY_MODE_LARGEST_ONLY; }
            else if (key == 's') { m_motionDisplayMode = MOTION_DISPLAY_MODE_SORT_BY_AREA_ALL; }
            else if (key == 'd') { m_motionDisplayMode = MOTION_DISPLAY_MODE_SORT_BY_AREA_MOTION; }
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
            else if (key >= '0' && key <= '6') {
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

    std::cout << "stopping all readers" << std::endl;
    for (auto& reader : m_readers) {
        reader->stop();
    }
    std::cout << "stopped all readers" << std::endl;

    std::cout << "close all wins" << std::endl;
    cv::destroyAllWindows();
    std::cout << "closed all wins" << std::endl;
}

std::string MotionDetector::bool_to_str(bool b)
{
    return std::string(b ? "Yes" : "No");
}
