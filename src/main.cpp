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

#ifdef DEBUG_VERBOSE
#include "debug.hpp"
#endif

int main(int argc, char* argv[])
{

    // Add signal handling
    std::signal(SIGINT, [](int) {
        cv::destroyAllWindows();
        std::exit(0);
    });

#ifdef DEBUG_VERBOSE
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
    program.add_argument("-a", "--area")
        .help("Contour area for detection")
        .default_value(MOTION_DETECT_AREA)
        .scan<'i', int>();
    program.add_argument("-ww", "--width")
        .help("Window width")
        .default_value(1920)
        .scan<'i', int>();
    program.add_argument("-wh", "--height")
        .help("Window height")
        .default_value(1080)
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
            MotionDetector motionDetector(
                program.get<std::string>("ip"),
                program.get<std::string>("username"),
                program.get<std::string>("password"),
                program.get<int>("area"), width, height, program.get<bool>("fullscreen"));

            motionDetector.draw_loop();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
