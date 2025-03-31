#pragma once

#include <atomic>
#include <opencv2/bgsegm.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "frame_reader.hpp"
#include "ring_buffer.hpp"

class MotionDetector {

  public:
    MotionDetector(const std::string& ip,
                   const std::string& username,
                   const std::string& password,
                   int width,
                   int height,
                   bool fullscreen,
                   int display_mode,
                   int area,
                   int rarea,
                   int current_channel,
                   int enable_motion,
                   int enable_motion_zoom_largest,
                   int enable_tour,
                   int enable_info,
                   int enable_minimap,
                   int enable_minimap_fullscreen,
                   int enable_fullscreen_channel);

    void draw_loop();
    void stop();

  private:
    // detecting
    void detect_motion();
    std::vector<std::vector<cv::Point>> find_contours_frame0();
    void detect_largest_motion_area_set_channel();

    void change_channel(int ch);
    void do_tour_logic();

    // drawing
    void draw_minimap();
    void draw_info();
    cv::Mat paint_main_mat_all();
    cv::Mat paint_main_mat_sort();
    cv::Mat paint_main_mat_multi();
    cv::Mat paint_main_mat_king();
    cv::Mat paint_main_mat_top();
    std::string bool_to_str(bool b);
    void handle_keys();

    std::atomic<bool> m_running{false};

    // set from params
    int m_display_width;
    int m_display_height;
    bool m_fullscreen;
    int m_display_mode;
    int m_motion_min_area;
    int m_motion_min_rect_area;
    std::atomic<int> m_current_channel;
    std::atomic<bool> m_enable_motion;
    std::atomic<bool> m_enable_motion_zoom_largest;
    std::atomic<bool> m_enable_tour;
    std::atomic<bool> m_enable_info;
    std::atomic<bool> m_enable_minimap;
    std::atomic<bool> m_enable_minimap_fullscreen;
    std::atomic<bool> m_enable_fullscreen_channel;

    // init
    std::thread m_thread_detect_motion;
    std::vector<std::unique_ptr<FrameReader>> m_readers;
    // cv::Ptr<cv::BackgroundSubtractorMOG2> m_fgbg;     // 69.6
    cv::Ptr<cv::BackgroundSubtractorKNN> m_fgbg; // 69% KNN
    // cv::Ptr<cv::bgsegm::BackgroundSubtractorCNT> m_fgbg; // 62% CNT

    cv::Mat m_frame0;
    DoubleBufferMat m_frame0_dbuff;
    cv::Mat m_canv3x3;
    cv::Mat m_canv3x2;
    cv::Mat m_main_display;

    // motion detecting / min frames
    std::atomic<bool> m_motion_detected{false};
    std::atomic<bool> m_motion_detected_min_ms{false};
    std::chrono::high_resolution_clock::time_point m_motion_detect_start;
    bool m_motion_detect_start_set;

    // motion linger
    bool m_motion_detect_linger{false};
    std::chrono::high_resolution_clock::time_point m_motion_detect_linger_start;
    bool m_motion_detect_linger_start_set;

    // draw & motion sleeps
    long long int m_draw_sleep_ms{0};
    long long int m_motion_sleep_ms{0};

    // tour
    std::atomic<int> m_tour_current_channel{1};
    std::chrono::high_resolution_clock::time_point m_tour_start;
    bool m_tour_start_set{false};

    DoubleBufferVec m_king_chain{{1, 2, 3, 4, 5, 6}};
    void move_to_front(int value);
};
