#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <opencv2/bgsegm.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "buffers.hpp"
#include "frame_reader.hpp"

#include "globals.hpp"

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
                   int motion_detect_min_ms,
                   int current_channel,
                   int enable_motion,
                   int enable_motion_zoom_largest,
                   int enable_tour,
                   int enable_info,
                   int tour_ms,
                   int enable_info_line,
                   int enable_info_rect,
                   int enable_minimap,
                   int enable_minimap_fullscreen,
                   int enable_fullscreen_channel,
                   int enable_ignore_contours,
                   int enable_alarm_pixels,
                   const std::string& ignore_contours,
                   const std::string& ignore_contours_file,
                   const std::string& alarm_pixels,
                   const std::string& alarm_pixels_file,
                   int focus_channel,
                   const std::string& focus_channel_area,
                   int focus_channel_sound);

    void draw_loop();
    void stop();
    DoubleBuffer<cv::Point> m_mouse_pos;

  private:
    // detecting
    void detect_motion();
    void detect_largest_motion_area_set_channel();

    void change_channel(int ch);
    void do_tour_logic();

    // drawing
    void draw_minimap();
    void draw_info();
    void draw_info_line();
    void draw_motion_region(cv::Mat canv, size_t posX, size_t posY, size_t width, size_t height);
    cv::Mat paint_main_mat_all();
    cv::Mat paint_main_mat_sort();
    cv::Mat paint_main_mat_multi();
    cv::Mat paint_main_mat_king();
    cv::Mat paint_main_mat_top();
    std::string bool_to_str(bool b);
    void handle_keys();

    // make ignore area
    void parse_ignore_contours(const std::string& input);
    void parse_ignore_contours_file(const std::string& filename);
    void print_ignore_contours();

    void parse_alarm_pixels(const std::string& input);
    void parse_alarm_pixels_file(const std::string& filename);
    void print_alarm_pixels();

    std::tuple<long, long, long, long> parse_area(const std::string& input);

    std::atomic<bool> m_running{true};

    // set from params
    int m_display_width;
    int m_display_height;
    bool m_fullscreen;
    std::atomic<int> m_display_mode;
    int m_motion_min_area;
    int m_motion_min_rect_area;
    int m_motion_detect_min_ms;
    int m_tour_ms;
    std::atomic<int> m_current_channel;
    std::atomic<bool> m_enable_motion;
    std::atomic<bool> m_enable_motion_zoom_largest;
    std::atomic<bool> m_enable_tour;
    std::atomic<bool> m_enable_info;
    std::atomic<bool> m_enable_info_line;
    std::atomic<bool> m_enable_info_rect;
    std::atomic<bool> m_enable_minimap;
    std::atomic<bool> m_enable_minimap_fullscreen;
    std::atomic<bool> m_enable_fullscreen_channel;
    std::atomic<bool> m_enable_ignore_contours;
    std::atomic<bool> m_enable_alarm_pixels;
    std::atomic<int> m_focus_channel{FOCUS_CHANNEL};
    std::atomic<bool> m_focus_channel_area_set;
    std::atomic<long> m_focus_channel_area_x;
    std::atomic<long> m_focus_channel_area_y;
    std::atomic<long> m_focus_channel_area_w;
    std::atomic<long> m_focus_channel_area_h;
    std::atomic<int> m_focus_channel_sound;

    // init
    std::thread m_thread_detect_motion;
    std::vector<std::unique_ptr<FrameReader>> m_readers;
    // cv::Ptr<cv::BackgroundSubtractorMOG2> m_fgbg;     // 69.6
    cv::Ptr<cv::BackgroundSubtractorKNN> m_fgbg; // 69% KNN
    // cv::Ptr<cv::bgsegm::BackgroundSubtractorCNT> m_fgbg; // 62% CNT

    cv::Mat m_frame0;
    DoubleBufferMat m_frame0_dbuff;
    cv::Mat m_canv1;
    cv::Mat m_canv2;
    cv::Mat m_main_display;

    // motion detecting / min frames
    LockFreeRingBuffer<cv::Rect, 2> m_motion_region;
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

    DoubleBufferVec m_king_chain{{1, 2, 3, 4, 5, 6, 7, 8}};
    void move_to_front(int value);
    std::atomic<int> m_layout_changed{false};

    std::mutex m_mtx_draw;
    std::condition_variable m_cv_draw;

    std::mutex m_mtx_motion;
    std::condition_variable m_cv_motion;

    // ignore area
    DoubleBuffer<std::vector<std::vector<cv::Point>>> m_ignore_contours;
    DoubleBuffer<std::vector<cv::Point>> m_ignore_contour;

    // alarm pixels
    DoubleBuffer<std::vector<cv::Point>> m_alarm_pixels;
};
