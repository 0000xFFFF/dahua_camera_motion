#include "motion_detector.hpp"
#include "debug.hpp"
#include "globals.hpp"
#include "opencv2/highgui.hpp"
#include <SDL2/SDL_mixer.h>
#include <argparse/argparse.hpp>
#include <iostream>
#include <opencv2/bgsegm.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/types.h>
#include <unistd.h>

MotionDetector::MotionDetector(const MotionDetectorParams& params)
    : m_subtype(params.subtype),
      m_display_width(params.width),
      m_display_height(params.height),
      m_fullscreen(params.fullscreen),
      m_display_mode(params.display_mode),
      m_motion_min_area(params.area),
      m_motion_min_rect_area(params.rarea),
      m_motion_detect_min_ms(params.motion_detect_min_ms),
      m_tour_ms(params.tour_ms),
      m_low_cpu(params.low_cpu),
      m_current_channel(params.current_channel),
      m_enable_motion(params.enable_motion),
      m_enable_motion_zoom_largest(params.enable_motion_zoom_largest),
      m_enable_tour(params.enable_tour),
      m_enable_info(params.enable_info),
      m_enable_info_line(params.enable_info_line),
      m_enable_info_rect(params.enable_info_rect),
      m_enable_minimap(params.enable_minimap),
      m_enable_minimap_fullscreen(params.enable_minimap_fullscreen),
      m_enable_fullscreen_channel(params.enable_fullscreen_channel),
      m_enable_ignore_contours(params.enable_ignore_contours),
      m_enable_alarm_pixels(params.enable_alarm_pixels),
      m_focus_channel(params.focus_channel),
      m_focus_channel_sound(params.focus_channel_sound),
      m_canv1(cv::Mat(cv::Size(params.width, params.height), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_canv2(cv::Mat(cv::Size(params.width, params.height), CV_8UC3, cv::Scalar(0, 0, 0))),
      m_main_display(cv::Mat(cv::Size(params.width, params.height), CV_8UC3, cv::Scalar(0, 0, 0)))

{
    init_ignore_contours(params);
    init_alarm_pixels(params);

    // m_fgbg = cv::createBackgroundSubtractorMOG2(20, 32, true);
    m_fgbg = cv::createBackgroundSubtractorKNN(20, 400.0, true);
    // m_fgbg = cv::bgsegm::createBackgroundSubtractorCNT(true, 15, true);

    // clang-format off
    if (params.low_cpu)                  { init_lowcpu(params);  }
    else if (params.focus_channel == -1) { init_default(params); }
    else                                 { init_focus(params);   }
    // clang-format on

    m_thread_ch0 = std::thread([this]() { update_ch0(); });
    m_thread_detect_motion = std::thread([this]() { detect_motion(); });
}

void MotionDetector::init_ignore_contours(const MotionDetectorParams& params)
{
    if (!params.ignore_contours.empty()) parse_ignore_contours(params.ignore_contours);
    if (!params.ignore_contours_file.empty()) parse_ignore_contours_file(params.ignore_contours_file);
    print_ignore_contours();
}

void MotionDetector::init_alarm_pixels(const MotionDetectorParams& params)
{
    if (!params.alarm_pixels.empty()) parse_alarm_pixels(params.alarm_pixels);
    if (!params.alarm_pixels_file.empty()) parse_alarm_pixels_file(params.alarm_pixels_file);
    print_alarm_pixels();
}

void MotionDetector::init_default(const MotionDetectorParams& params)
{
    m_readers.emplace_back(std::make_unique<FrameReader>(0, params.ip, params.username, params.password, params.subtype, true));
    for (int channel = 1; channel <= CHANNEL_COUNT; ++channel) {
        m_readers.emplace_back(std::make_unique<FrameReader>(channel, params.ip, params.username, params.password, params.subtype, true));
    }

    change_channel(params.current_channel);
}

void MotionDetector::init_lowcpu(const MotionDetectorParams& params)
{
    m_readers.emplace_back(std::make_unique<FrameReader>(0, params.ip, params.username, params.password, params.subtype, true));
    for (int channel = 1; channel <= CHANNEL_COUNT; ++channel) {
        m_readers.emplace_back(std::make_unique<FrameReader>(channel, params.ip, params.username, params.password, params.subtype, false));
    }

    change_channel(params.current_channel);
}

void MotionDetector::init_focus(const MotionDetectorParams& params)
{
    for (int channel = 0; channel <= CHANNEL_COUNT; channel++) {
        m_readers.emplace_back(std::make_unique<FrameReader>(channel, params.ip, params.username, params.password, params.subtype, channel == params.focus_channel));
    }

    if (!params.focus_channel_area.empty() && params.focus_channel_area != "") {
        auto [x, y, w, h] = parse_area(params.focus_channel_area);
        m_focus_channel_area_x = x;
        m_focus_channel_area_y = y;
        m_focus_channel_area_w = w;
        m_focus_channel_area_h = h;
        m_focus_channel_area_set = true;
        D(std::cout << "focus channel area: "
                    << m_focus_channel_area_x << "x"
                    << m_focus_channel_area_y << ";"
                    << m_focus_channel_area_w << "x"
                    << m_focus_channel_area_h << std::endl;);
    }
}

void MotionDetector::stop()
{
    std::cout << "\n\nQuitting..." << std::endl;
    m_running = false;
    m_cv_draw.notify_one();
    m_cv_motion.notify_one();
    if (m_thread_detect_motion.joinable()) { m_thread_detect_motion.join(); }
    for (auto& reader : m_readers) { reader->stop(); }
    D(std::cout << "destroy all win" << std::endl);
    cv::destroyAllWindows();
    D(std::cout << "destroy all win done" << std::endl);
}
