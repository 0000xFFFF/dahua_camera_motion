#include "motion_detector_params.hpp"
#include "debug.hpp"
#include "globals.hpp"
#include "utils.hpp"

MotionDetectorParams::MotionDetectorParams(std::unique_ptr<argparse::ArgumentParser>& program)
    : // clang-format off
    ip                         {program->get<std::string>("ip")},
    username                   {program->get<std::string>("username")},
    password                   {program->get<std::string>("password")},
    subtype                    {program->get<int>("subtype")},
    width                      {program->get<int>("width")},
    height                     {program->get<int>("height")},
    fullscreen                 {program->get<bool>("fullscreen")},
    display_mode               {program->get<int>("display_mode")},
    area                       {program->get<int>("area")},
    rarea                      {program->get<int>("rarea")},
    motion_detect_min_ms       {program->get<int>("motion_detect_min_ms")},
    current_channel            {program->get<int>("current_channel")},
    enable_motion              {program->get<int>("enable_motion")},
    enable_motion_zoom_largest {program->get<int>("enable_motion_zoom_largest")},
    sleep_ms_draw              {program->get<int>("sleep_ms_draw")},
    sleep_ms_motion            {program->get<int>("sleep_ms_motion")},
    enable_tour                {program->get<int>("enable_tour")},
    tour_ms                    {program->get<int>("tour_ms")},
    enable_info                {program->get<int>("enable_info")},
    enable_info_line           {program->get<int>("enable_info_line")},
    enable_info_rect           {program->get<int>("enable_info_rect")},
    enable_minimap             {program->get<int>("enable_minimap")},
    enable_minimap_fullscreen  {program->get<int>("enable_minimap_fullscreen")},
    enable_fullscreen_channel  {program->get<int>("enable_fullscreen_channel")},
    enable_ignore_contours     {program->get<int>("enable_ignore_contours")},
    enable_alarm_pixels        {program->get<int>("enable_alarm_pixels")},
    ignore_contours            {program->get<std::string>("ignore_contours")},
    ignore_contours_file       {program->get<std::string>("ignore_contours_file")},
    alarm_pixels               {program->get<std::string>("alarm_pixels")},
    alarm_pixels_file          {program->get<std::string>("alarm_pixels_file")},
    focus_channel              {program->get<int>("focus_channel")},
    focus_channel_area         {program->get<std::string>("focus_channel_area")},
    focus_channel_sound        {program->get<int>("focus_channel_sound")},
    low_cpu                    {program->get<int>("low_cpu")},
    low_cpu_hq_motion          {program->get<int>("low_cpu_hq_motion")},
    low_cpu_hq_motion_dual     {program->get<int>("low_cpu_hq_motion_dual")}
// clang-format on
{

    if (program->get<bool>("ignore_alarm_make")) {
        width = W_0;
        height = H_0;
        enable_minimap_fullscreen = 1;
        enable_ignore_contours = 1;
        enable_alarm_pixels = 1;

        std::cout << "to create ignore area use keys:\n"
                  << "   c - create vertex\n"
                  << "   v - join contour\n"
                  << "   b - clear all" << std::endl;

        std::cout << "to create alarm pixels:\n"
                  << "   x - create alarm pixel\n"
                  << "   z - clear all" << std::endl;
    }

    if (program->get<bool>("detect")) {
        auto [detected_width, detected_height] = detect_screen_size(program->get<int>("resolution"));
        width = detected_width;
        height = detected_height;
        std::cout << "Detected screen size: " << width << "x" << height << std::endl;
    }

    if (low_cpu_hq_motion) { low_cpu = 1; }
    if (low_cpu_hq_motion_dual) {
        low_cpu = 1;
        low_cpu_hq_motion = 1;
    }

    if (sleep_ms_draw == -1) { sleep_ms_draw_auto = true; }
    if (sleep_ms_motion == -1) { sleep_ms_motion_auto = true; }

    // clang-format off
    D(std::cout << "ip                        = " << ip                         << std::endl);
    D(std::cout << "username                  = " << username                   << std::endl);
    D(std::cout << "password                  = " << password                   << std::endl);
    D(std::cout << "width                     = " << width                      << std::endl);
    D(std::cout << "height                    = " << height                     << std::endl);
    D(std::cout << "fullscreen                = " << fullscreen                 << std::endl);
    D(std::cout << "display_mode              = " << display_mode               << std::endl);
    D(std::cout << "area                      = " << area                       << std::endl);
    D(std::cout << "rarea                     = " << rarea                      << std::endl);
    D(std::cout << "motion_detect_min_ms      = " << motion_detect_min_ms       << std::endl);
    D(std::cout << "current_channel           = " << current_channel            << std::endl);
    D(std::cout << "enable_motion             = " << enable_motion              << std::endl);
    D(std::cout << "enable_motion_zoom_larges = " << enable_motion_zoom_largest << std::endl);
    D(std::cout << "enable_tour               = " << enable_tour                << std::endl);
    D(std::cout << "sleep_ms_draw             = " << sleep_ms_draw              << " (auto: " << sleep_ms_draw_auto << ")" << std::endl);
    D(std::cout << "sleep_ms_motion           = " << sleep_ms_motion            << " (auto: " << sleep_ms_motion_auto << ")" << std::endl);
    D(std::cout << "tour_ms                   = " << tour_ms                    << std::endl);
    D(std::cout << "enable_info               = " << enable_info                << std::endl);
    D(std::cout << "enable_info_line          = " << enable_info_line           << std::endl);
    D(std::cout << "enable_info_rect          = " << enable_info_rect           << std::endl);
    D(std::cout << "enable_minimap            = " << enable_minimap             << std::endl);
    D(std::cout << "enable_minimap_fullscreen = " << enable_minimap_fullscreen  << std::endl);
    D(std::cout << "enable_fullscreen_channel = " << enable_fullscreen_channel  << std::endl);
    D(std::cout << "enable_ignore_contours    = " << enable_ignore_contours     << std::endl);
    D(std::cout << "enable_alarm_pixels       = " << enable_alarm_pixels        << std::endl);
    D(std::cout << "ignore_contours           = " << ignore_contours            << std::endl);
    D(std::cout << "ignore_contours_file      = " << ignore_contours_file       << std::endl);
    D(std::cout << "alarm_pixels              = " << alarm_pixels               << std::endl);
    D(std::cout << "alarm_pixels_file         = " << alarm_pixels_file          << std::endl);
    D(std::cout << "focus_channel             = " << focus_channel              << std::endl);
    D(std::cout << "focus_channel_area        = " << focus_channel_area         << std::endl);
    D(std::cout << "focus_channel_sound       = " << focus_channel_sound        << std::endl);
    D(std::cout << "low_cpu                   = " << low_cpu                    << std::endl);
    D(std::cout << "low_cpu_hq_motion         = " << low_cpu_hq_motion          << std::endl);
    D(std::cout << "low_cpu_hq_motion_dual    = " << low_cpu_hq_motion_dual     << std::endl);
    // clang-format on
}
