#include "debug.hpp"
#include "motion_detector.hpp"
#include <fstream>

cv::Mat MotionDetector::get_frame(int channel, int layout_changed)
{
    if (m_low_cpu) {
        if (m_low_cpu_hq_motion) {
            if (m_readers[channel]->is_running() && m_readers[channel]->is_active()) {
                return m_readers[channel]->get_latest_frame(layout_changed);
            }
        }

        // get frame fro ch 0
        constexpr int mini_ch_w = W_0 / 3;
        constexpr int mini_ch_h = H_0 / 3;

        cv::Mat frame0 = m_frame0_dbuff.get();
        if (frame0.empty()) { return frame0; }

        int row = (channel - 1) / 3; // groups of 3 channels
        if (channel >= 7) row = 2;   // adjust since only 2 channels in last row
        int col = (channel - 1) % 3; // column index
        return frame0(cv::Rect(mini_ch_w * col, mini_ch_h * row, mini_ch_w, mini_ch_h));
    }

    return m_readers[channel]->get_latest_frame(layout_changed);
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

        // fight to top
        m_tour_current_channel++;
        if (m_tour_current_channel > CHANNEL_COUNT) { m_tour_current_channel = 1; }
        change_channel(m_tour_current_channel);
    }
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
