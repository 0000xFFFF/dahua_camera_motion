#pragma once

#include <atomic>
#include <opencv2/bgsegm.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "frame_reader.hpp"
#include "globals.hpp"
#include "ring_buffer.hpp"

class MotionDetector {

  public:
    MotionDetector(const std::string& ip, const std::string& username,
                   const std::string& password, int area, int w, int h, bool fullscreen);

    void draw_loop();
    void stop();

  private:
    // detecting
    void detect_motion();
    std::vector<std::vector<cv::Point>> find_contours_frame0();
    void detect_largest_motion_area_set_channel();

    void do_tour_logic();

    // drawing
    void draw_minimap();
    void draw_info();
    cv::Mat paint_main_mat_all();
    cv::Mat paint_main_mat_sort();
    cv::Mat paint_main_mat_multi();
    cv::Mat paint_main_mat_king();
    cv::Mat paint_main_mat_king(const std::list<int>& chs);
    cv::Mat paint_main_mat_top();
    std::string bool_to_str(bool b);
    void handle_keys();

    // set from params
    int m_motion_area;
    int m_display_width;
    int m_display_height;
    bool m_fullscreen;
    std::atomic<bool> m_running{false};

    // init
    std::thread m_thread_detect_motion;
    std::vector<std::unique_ptr<FrameReader>> m_readers;
    // cv::Ptr<cv::BackgroundSubtractorMOG2> m_fgbg;     // 69.6
    cv::Ptr<cv::BackgroundSubtractorKNN> m_fgbg; // 69% KNN
    // cv::Ptr<cv::bgsegm::BackgroundSubtractorCNT> m_fgbg; // 62% CNT

    // defaults
    std::atomic<bool> m_enableMotion{ENABLE_MOTION};
    std::atomic<int> m_current_channel{1};
    std::atomic<int> m_motion_ch{1};
    std::atomic<int> m_motion_ch_frames{0};

    bool m_enableInfo = ENABLE_INFO;
    int m_motionDisplayMode = MOTION_DISPLAY_MODE;
    bool m_enableMinimap = ENABLE_MINIMAP;
    bool m_enableMinimapFullscreen = ENABLE_MINIMAP_FULLSCREEN;
    bool m_enableFullscreenChannel = ENABLE_FULLSCREEN_CHANNEL;
    bool m_enableTour = ENABLE_TOUR;

    cv::Mat m_frame0;
    DoubleBufferMat m_frame0_dbuff;
    cv::Mat m_canv;
    cv::Mat m_main_display;

    std::atomic<bool> m_motion_detected{false};
    long long int m_motion_sleep_ms{0};
    std::atomic<int> m_motion_detect_min_frames{0};
    std::atomic<bool> m_motion_detected_min_frames{false};

    long long int m_draw_sleep_ms{0};
    std::atomic<int> m_tour_frame_count{0};
    std::atomic<int> m_tour_frame_index{0};

    DoubleBufferList m_sorted_chs_area_all;
    void move_to_front(int value);
};
