#include <iostream>
#include <opencv2/opencv.hpp>
#include <sched.h>
#include <string>

#include "args.hpp"
#include "debug.hpp"
#include "motion_detector.hpp"
#include "utils.hpp"

#include "signal.hpp"
#include "sound.hpp"

#ifdef DEBUG_CPU
#include "debug.hpp"
#endif

std::unique_ptr<MotionDetector> motionDetector;

int main(int argc, char* argv[])
{
    init_signal();

#ifdef DEBUG_CPU
    CpuUsageMonitor monitor;
#endif

    try {
        auto program = parse_args();
        program->parse_args(argc, argv);

        init_sound();

        auto [width, height] = get_width_height(
            program->get<int>("width"),
            program->get<int>("height"),
            program->get<bool>("detect"),
            program->get<int>("resolution"));

        {
            motionDetector = std::make_unique<MotionDetector>(
                program->get<std::string>("ip"),
                program->get<std::string>("username"),
                program->get<std::string>("password"),
                width,
                height,
                program->get<bool>("fullscreen"),
                program->get<int>("display_mode"),
                program->get<int>("area"),
                program->get<int>("rarea"),
                program->get<int>("motion_detect_min_ms"),
                program->get<int>("current_channel"),
                program->get<int>("enable_motion"),
                program->get<int>("enable_motion_zoom_largest"),
                program->get<int>("enable_tour"),
                program->get<int>("tour_ms"),
                program->get<int>("enable_info"),
                program->get<int>("enable_info_line"),
                program->get<int>("enable_info_rect"),
                program->get<int>("enable_minimap"),
                program->get<int>("enable_minimap_fullscreen"),
                program->get<int>("enable_fullscreen_channel"),
                program->get<int>("enable_ignore_contours"),
                program->get<int>("enable_alarm_pixels"),
                program->get<std::string>("ignore_contours"),
                program->get<std::string>("ignore_contours_file"),
                program->get<std::string>("alarm_pixels"),
                program->get<std::string>("alarm_pixels_file"),
                program->get<int>("focus_channel"),
                program->get<std::string>("focus_channel_area"),
                program->get<int>("focus_channel_sound"),
                program->get<int>("low_cpu")

            );

            motionDetector->draw_loop();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    uninit_sound();

    DPL("main return 0");
    return 0;
}
