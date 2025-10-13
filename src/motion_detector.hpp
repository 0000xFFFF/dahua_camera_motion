#pragma once
#include <argparse/argparse.hpp>
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

#include "motion_detector_params.hpp"

class MotionDetector {

  public:
    MotionDetector(const MotionDetectorParams& params);

    void draw_loop();
    void stop();
    DoubleBuffer<cv::Point> m_mouse_pos;

  private:
    void init_default(const MotionDetectorParams& params);
    void init_lowcpu(const MotionDetectorParams& params);
    void init_focus(const MotionDetectorParams& params);

    void init_ignore_contours(const MotionDetectorParams& params);
    void init_alarm_pixels(const MotionDetectorParams& params);

    void detect_motion();
    void update_ch0();
    void detect_largest_motion_area_set_channel();

    void change_channel(int ch);
    void do_tour_logic();

    void draw_loop_handle_keys();

    void draw_paint_info_minimap();
    void draw_paint_info_text();
    void draw_paint_info_line();
    void draw_paint_info_motion_region(cv::Mat canv, size_t posX, size_t posY, size_t width, size_t height);

    cv::Mat draw_paint_main_mat_all();
    cv::Mat draw_paint_main_mat_sort();
    cv::Mat draw_paint_main_mat_multi();
    cv::Mat draw_paint_main_mat_king();
    cv::Mat draw_paint_main_mat_top();

    void parse_ignore_contours(const std::string& input);
    void parse_ignore_contours_file(const std::string& filename);
    void print_ignore_contours();

    void parse_alarm_pixels(const std::string& input);
    void parse_alarm_pixels_file(const std::string& filename);
    void print_alarm_pixels();

    std::tuple<long, long, long, long> parse_area(const std::string& input);

    cv::Mat get_frame(int channel, int layout_changed);

    std::atomic<bool> m_running{true};

    int m_subtype;
    int m_display_width;
    int m_display_height;
    bool m_fullscreen;
    std::atomic<int> m_display_mode;
    int m_motion_min_area;
    int m_motion_min_rect_area;
    int m_motion_detect_min_ms;
    int m_tour_ms;
    int m_low_cpu;
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
    std::thread m_thread_ch0;
    std::thread m_thread_detect_motion;
    std::vector<std::unique_ptr<FrameReader>> m_readers;
    // cv::Ptr<cv::BackgroundSubtractorMOG2> m_fgbg;     // 69.6
    cv::Ptr<cv::BackgroundSubtractorKNN> m_fgbg; // 69% KNN
    // cv::Ptr<cv::bgsegm::BackgroundSubtractorCNT> m_fgbg; // 62% CNT

    cv::Mat m_frame0;
    DoubleBufferMat m_frame0_dbuff;
    cv::Mat m_frame_detection;
    DoubleBufferMat m_frame_detection_dbuff;
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
    long long int m_ch0_sleep_ms{0};

    // tour
    std::atomic<int> m_tour_current_channel{1};
    std::chrono::high_resolution_clock::time_point m_tour_start;
    bool m_tour_start_set{false};

    DoubleBufferVec m_king_chain{{1, 2, 3, 4, 5, 6, 7, 8}};
    void move_to_front(int value);
    std::atomic<int> m_layout_changed{false};

    std::mutex m_mtx_draw;
    std::condition_variable m_cv_draw;

    std::mutex m_mtx_ch0;
    std::condition_variable m_cv_ch0;

    std::mutex m_mtx_motion;
    std::condition_variable m_cv_motion;

    // ignore area
    DoubleBuffer<std::vector<std::vector<cv::Point>>> m_ignore_contours;
    DoubleBuffer<std::vector<cv::Point>> m_ignore_contour;

    // alarm pixels
    DoubleBuffer<std::vector<cv::Point>> m_alarm_pixels;
};
