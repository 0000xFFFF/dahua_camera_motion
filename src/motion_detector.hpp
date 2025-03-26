#pragma once

#include <atomic>
#include <opencv2/bgsegm.hpp>
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
    std::vector<std::vector<cv::Point>> find_contours_frame0();
    void detect_largest_motion_area_set_channel();
    void sort_channels_by_motion_area_motion_channels();

    void draw_minimap();
    void draw_info();
    cv::Mat paint_main_mat_all();
    cv::Mat paint_main_mat_sort();
    cv::Mat paint_main_mat_multi();
    cv::Mat paint_main_mat_king();
    cv::Mat paint_main_mat_king(const std::list<int>& chs);
    cv::Mat paint_main_mat_top();
    std::string bool_to_str(bool b);
    cv::Mat frame_get_non_empty(const int& i);
    void handle_keys();

    // set from params
    int m_motion_area;
    int m_display_width;
    int m_display_height;
    bool m_fullscreen;
    std::atomic<bool> m_running{false};

    // init
    std::vector<std::unique_ptr<FrameReader>> m_readers;
    // cv::Ptr<cv::BackgroundSubtractorMOG2> m_fgbg;
    // cv::Ptr<cv::BackgroundSubtractorKNN> m_fgbg;
    cv::Ptr<cv::bgsegm::BackgroundSubtractorCNT> m_fgbg;

    // defaults
    int m_current_channel = 1;
    int m_motion_ch = 1;
    int m_motion_ch_frames = 0;
    bool m_enableInfo = ENABLE_INFO;
    bool m_enableMotion = ENABLE_MOTION;
    int m_motionDisplayMode = MOTION_DISPLAY_MODE;
    bool m_enableMinimap = ENABLE_MINIMAP;
    bool m_enableMinimapFullscreen = ENABLE_MINIMAP_FULLSCREEN;
    bool m_enableFullscreenChannel = ENABLE_FULLSCREEN_CHANNEL;
    bool m_enableTour = ENABLE_TOUR;

    cv::Mat m_frame0 = cv::Mat::zeros(W_0, H_0, CV_8UC3);
    cv::Mat m_main_c3r2 = cv::Mat(cv::Size(W_HD * 3, H_HD * 2), CV_8UC3, cv::Scalar(0, 0, 0)); // all
    cv::Mat m_main_c1r1 = cv::Mat::zeros(W_HD, H_HD, CV_8UC3);                                 // fullscreenCh, tour
    cv::Mat m_main_c3r3;                                                                       // KING, TOP

    cv::Rect m_motion_region;
    bool m_motion_detected = false;
    bool m_motion_detected_min_frames = false;

    int m_tour_frame_index = 0;

    // Stores all channels with their motion areas (motion chs + non motion chs)
    std::list<int> m_sorted_chs_area_all = {1, 2, 3, 4, 5, 6};
    void move_to_front(int value);

    // Stores only motion channels with their areas
    std::vector<std::pair<int, double>> m_sorted_chs_area_motion;
};
