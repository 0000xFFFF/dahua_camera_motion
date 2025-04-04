#include <argparse/argparse.hpp>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <sched.h>
#include <string>

#include "globals.hpp"
#include "motion_detector.hpp"
#include "utils.h"

#ifdef DEBUG_CPU
#include "debug.hpp"
#endif

std::unique_ptr<MotionDetector> motionDetector;

int main(int argc, char* argv[])
{
    // Add signal handling
    std::signal(SIGINT, [](int) {
        motionDetector->stop();
        motionDetector.release();
        std::cout << "exit" << std::endl;
        std::exit(0);
    });

#ifdef DEBUG_CPU
    CpuUsageMonitor monitor;
#endif

    argparse::ArgumentParser program("dcm_master");

    program.add_argument("-i", "--ip")
        .help("IP to connect to")
        .required();
    program.add_argument("-u", "--username")
        .help("Account username")
        .required();
    program.add_argument("-p", "--password")
        .help("Account password")
        .required();

    program.add_argument("-ww", "--width")
        .help("Window width")
        .default_value(DEFAULT_WIDTH)
        .scan<'i', int>();
    program.add_argument("-wh", "--height")
        .help("Window height")
        .default_value(DEFAULT_HEIGHT)
        .scan<'i', int>();
    program.add_argument("-fs", "--fullscreen")
        .help("Start in fullscreen mode")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-d", "--detect")
        .help("Detect screen size with xrandr")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-r", "--resolution")
        .help("index of resolution to use (default: 0)")
        .default_value(0)
        .scan<'i', int>();

    program.add_argument("-dm", "--display_mode")
        .help("display mode for cameras (0 = single, 1 = all, 2 = sort, 3 = king, 4 = top")
        .metavar("0-4")
        .default_value(DISPLAY_MODE)
        .scan<'i', int>();

    program.add_argument("-a", "--area")
        .help("Min contour area for detection")
        .default_value(MOTION_DETECT_AREA)
        .scan<'i', int>();
    program.add_argument("-ra", "--rarea")
        .help("Min contour's bounding rectangle area for detection")
        .default_value(MOTION_DETECT_RECT_AREA)
        .scan<'i', int>();
    program.add_argument("-ms", "--motion_detect_min_ms")
        .help("min milliseconds of detected motion to switch channel")
        .default_value(MOTION_DETECT_MIN_MS)
        .scan<'i', int>();
    program.add_argument("-emzl", "--enable_motion_zoom_largest")
        .help("enable motion zoom largest")
        .metavar("0/1")
        .default_value(ENABLE_MOTION_ZOOM_LARGEST)
        .scan<'i', int>();

    program.add_argument("-ch", "--current_channel")
        .help("which channel to start with")
        .metavar("1-6")
        .default_value(CURRENT_CHANNEL)
        .scan<'i', int>();

    program.add_argument("-em", "--enable_motion")
        .help("enable motion")
        .metavar("0/1")
        .default_value(ENABLE_MOTION)
        .scan<'i', int>();
    program.add_argument("-et", "--enable_tour")
        .help("enable motion zoom largest")
        .metavar("0/1")
        .default_value(ENABLE_TOUR)
        .scan<'i', int>();
    program.add_argument("-ei", "--enable_info")
        .help("enable drawing info")
        .metavar("0/1")
        .default_value(ENABLE_INFO)
        .scan<'i', int>();
    program.add_argument("-eil", "--enable_info_line")
        .help("enable drawing line info (motion, linger, tour, ...)")
        .metavar("0/1")
        .default_value(ENABLE_INFO_LINE)
        .scan<'i', int>();
    program.add_argument("-eir", "--enable_info_rect")
        .help("enable drawing motion rectangle")
        .metavar("0/1")
        .default_value(ENABLE_INFO_RECT)
        .scan<'i', int>();
    program.add_argument("-emm", "--enable_minimap")
        .help("enable minimap")
        .metavar("0/1")
        .default_value(ENABLE_MINIMAP)
        .scan<'i', int>();
    program.add_argument("-emf", "--enable_minimap_fullscreen")
        .help("enable minimap fullscreen")
        .metavar("0/1")
        .default_value(ENABLE_MINIMAP_FULLSCREEN)
        .scan<'i', int>();
    program.add_argument("-efc", "--enable_fullscreen_channel")
        .help("enable minimap fullscreen")
        .metavar("0/1")
        .default_value(ENABLE_FULLSCREEN_CHANNEL)
        .scan<'i', int>();

    program.add_argument("-eic", "--enable_ignore_contours")
        .help("enable ignoring contours/areas (specify with -ic)")
        .metavar("0/1")
        .default_value(ENABLE_IGNORE_CONTOURS)
        .scan<'i', int>();
    program.add_argument("-ic", "--ignore_contours")
        .help("specify ignore contours/areas (e.g.: <x>x<y>,...;<x>x<y>,...)")
        .default_value(IGNORE_CONTOURS);
    program.add_argument("-icf", "--ignore_contours_filename")
        .help("specify ignore contours/areas inside file (seperated by new line) (e.g.: <x>x<y>,...\\n<x>x<y>,...)")
        .default_value(IGNORE_CONTOURS_FILENAME);

    try {
        program.parse_args(argc, argv);

        int width = program.get<int>("width");
        int height = program.get<int>("height");

        // Override width/height with detected screen size if requested
        if (program.get<bool>("detect")) {
            auto [detected_width, detected_height] = detect_screen_size(program.get<int>("resolution"));
            width = detected_width;
            height = detected_height;
            std::cout << "Detected screen size: " << width << "x" << height << std::endl;
        }

        {
            motionDetector = std::make_unique<MotionDetector>(
                program.get<std::string>("ip"),
                program.get<std::string>("username"),
                program.get<std::string>("password"),
                width,
                height,
                program.get<bool>("fullscreen"),
                program.get<int>("display_mode"),
                program.get<int>("area"),
                program.get<int>("rarea"),
                program.get<int>("motion_detect_min_ms"),
                program.get<int>("current_channel"),
                program.get<int>("enable_motion"),
                program.get<int>("enable_motion_zoom_largest"),
                program.get<int>("enable_tour"),
                program.get<int>("enable_info"),
                program.get<int>("enable_info_line"),
                program.get<int>("enable_info_rect"),
                program.get<int>("enable_minimap"),
                program.get<int>("enable_minimap_fullscreen"),
                program.get<int>("enable_fullscreen_channel"),
                program.get<int>("enable_ignore_contours"),
                program.get<std::string>("ignore_contours"),
                program.get<std::string>("ignore_contours_filename")
                    );

            motionDetector->draw_loop();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
