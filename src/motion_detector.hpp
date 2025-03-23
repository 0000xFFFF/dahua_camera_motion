#pragma once

#include <atomic>
#include <opencv2/opencv.hpp>
#include <string>
#include <utility>
#include <vector>

#include "frame_reader.hpp"
#include "globals.hpp"

class MotionDetector {

  public:
    MotionDetector(const std::string& ip, const std::string& username,
                   const std::string& password, int area, int w, int h, bool fullscreen);

    void start();
    void stop();

  private:

    void do_tour_logic();
    std::vector<std::vector<cv::Point>> find_contours(cv::Mat& frame0);
    void detect_largest_motion_set_channel(cv::Mat& frame0);
    void detect_all_motion_set_channels(cv::Mat& frame0);

    void draw_minimap(cv::Mat& frame0, cv::Mat& main_frame);
    cv::Mat paint_main_mat_all(cv::Mat& main_mat);
    cv::Mat paint_main_mat_some(cv::Mat& main_mat);
    std::string bool_to_str(bool b);

    // set from params
    int m_motion_area;
    int m_display_width;
    int m_display_height;
    bool m_fullscreen;
    std::atomic<bool> m_running{false};

    // init
    std::vector<std::unique_ptr<FrameReader>> m_readers;
    cv::Ptr<cv::BackgroundSubtractorMOG2> m_fgbg;

    // defaults
    int m_current_channel = 1;
    bool m_enableInfo = ENABLE_INFO;
    bool m_enableMotion = ENABLE_MOTION;
    bool m_enableMotionLargest = ENABLE_MOTION_LARGEST;
    bool m_enableMotionAll = ENABLE_MOTION_ALL;
    bool m_enableMinimap = ENABLE_MINIMAP;
    bool m_enableFullscreen = ENABLE_FULLSCREEN;
    bool m_enableTour = ENABLE_TOUR;


    cv::Mat m_frame0_minimap;

    cv::Rect m_motion_region;
    bool m_motion_detected = false;
    bool m_motion_detected_min_frames = false;

    int m_tour_frame_count = TOUR_SLEEP_MS / DRAW_SLEEP_MS;
    int m_tour_frame_index = 0;

    int m_motion_detected_frame_count = 0;
    int m_motion_detected_frame_count_history = 0;

    std::vector<std::pair<int, double>> m_sorted_chs_area; // Stores channels with their areas

};
