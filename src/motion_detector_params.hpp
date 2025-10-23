#pragma once

#include <argparse/argparse.hpp>
#include <string>

struct MotionDetectorParams {
    std::string ip;
    std::string username;
    std::string password;
    int subtype;
    int width;
    int height;
    bool fullscreen;
    int display_mode;
    int area;
    int rarea;
    int motion_detect_min_ms;
    int current_channel;
    int enable_motion;
    int enable_motion_zoom_largest;
    int enable_tour;
    int tour_ms;
    int enable_info;
    int enable_info_line;
    int enable_info_rect;
    int enable_minimap;
    int enable_minimap_fullscreen;
    int enable_fullscreen_channel;
    int enable_ignore_contours;
    int enable_alarm_pixels;
    std::string ignore_contours;
    std::string ignore_contours_file;
    std::string alarm_pixels;
    std::string alarm_pixels_file;
    int focus_channel;
    std::string focus_channel_area;
    int focus_channel_sound;
    int low_cpu;
    int low_cpu_hq_motion;
    int low_cpu_hq_motion_dual;
    MotionDetectorParams(std::unique_ptr<argparse::ArgumentParser>& program);
};
