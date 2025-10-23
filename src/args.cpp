#include "globals.hpp"
#include "args.hpp"
#include "debug.hpp"

std::unique_ptr<argparse::ArgumentParser> parse_args() {

    auto program= std::make_unique<argparse::ArgumentParser>("dcm_master", VERSION);
    program->add_description("motion detection kiosk for dahua cameras");

    auto& options_required = program->add_group("Required Options");
    options_required.add_argument("-i", "--ip")
        .help("ip to connect to")
        .metavar("ip")
        .required();
    options_required.add_argument("-u", "--username")
        .help("account username")
        .metavar("username")
        .required();
    options_required.add_argument("-p", "--password")
        .help("account password")
        .metavar("password")
        .required();

    auto& options_window = program->add_group("Window Options");
    options_window.add_argument("-ww", "--width")
        .help("window width")
        .metavar("NUMBER")
        .default_value(DEFAULT_WIDTH)
        .scan<'i', int>();
    options_window.add_argument("-wh", "--height")
        .help("window height")
        .metavar("NUMBER")
        .default_value(DEFAULT_HEIGHT)
        .scan<'i', int>();
    options_window.add_argument("-fs", "--fullscreen")
        .help("start in fullscreen mode")
        .default_value(false)
        .implicit_value(true);
    options_window.add_argument("-d", "--detect")
        .help("detect screen size with xrandr")
        .default_value(false)
        .implicit_value(true);
    options_window.add_argument("-r", "--resolution")
        .help("index of resolution to use")
        .metavar("0,1,2,...")
        .default_value(0)
        .scan<'i', int>();

    auto& options_start = program->add_group("Start Options");
    options_start.add_argument("-st", "--subtype")
        .help("witch subtype to use (0 = full hq, 1 = smaller resolution)")
        .metavar("0/1")
        .default_value(static_cast<int>(SUBTYPE))
        .scan<'i', int>();
    options_start.add_argument("-dm", "--display_mode")
        .help("display mode for cameras (0 = single, 1 = all, 2 = sort, 3 = king, 4 = top")
        .metavar("0-4")
        .default_value(static_cast<int>(DISPLAY_MODE_DEFAULT))
        .scan<'i', int>();
    options_start.add_argument("-ch", "--current_channel")
        .help("which channel to start with")
        .metavar("1-8")
        .default_value(CURRENT_CHANNEL)
        .scan<'i', int>();
    options_start.add_argument("-efc", "--enable_fullscreen_channel")
        .help("enable fullscreen channel")
        .metavar("0/1")
        .default_value(ENABLE_FULLSCREEN_CHANNEL)
        .scan<'i', int>();

    auto& options_motion = program->add_group("Motion Detection Options");
    options_motion.add_argument("-em", "--enable_motion")
        .help("enable motion")
        .metavar("0/1")
        .default_value(ENABLE_MOTION)
        .scan<'i', int>();
    options_motion.add_argument("-a", "--area")
        .help("min contour area for motion detection")
        .metavar("0/1")
        .default_value(MOTION_DETECT_AREA)
        .scan<'i', int>();
    options_motion.add_argument("-ra", "--rarea")
        .help("min contour's bounding rectangle area for detection")
        .metavar("0/1")
        .default_value(MOTION_DETECT_RECT_AREA)
        .scan<'i', int>();
    options_motion.add_argument("-ms", "--motion_detect_min_ms")
        .help("minimum milliseconds of detected motion to switch channel")
        .metavar("NUMBER")
        .default_value(MOTION_DETECT_MIN_MS)
        .scan<'i', int>();
    options_motion.add_argument("-emzl", "--enable_motion_zoom_largest")
        .help("zoom channel on largest detected motion")
        .metavar("0/1")
        .default_value(ENABLE_MOTION_ZOOM_LARGEST)
        .scan<'i', int>();

    auto& options_tour = program->add_group("Tour Options");
    options_tour.add_argument("-et", "--enable_tour")
        .help("tour, switch channels every X ms (set with -tms)")
        .metavar("0/1")
        .default_value(ENABLE_TOUR)
        .scan<'i', int>();
    options_tour.add_argument("-tms", "--tour_ms")
        .help("how many ms to focus on a channel before switching")
        .metavar("NUMBER")
        .default_value(TOUR_MS)
        .scan<'i', int>();

    auto& options_info = program->add_group("Info Options");
    options_info.add_argument("-ei", "--enable_info")
        .help("enable drawing info")
        .metavar("0/1")
        .default_value(ENABLE_INFO)
        .scan<'i', int>();
    options_info.add_argument("-eil", "--enable_info_line")
        .help("enable drawing line info (motion, linger, tour, ...)")
        .metavar("0/1")
        .default_value(ENABLE_INFO_LINE)
        .scan<'i', int>();
    options_info.add_argument("-eir", "--enable_info_rect")
        .help("enable drawing motion rectangles and contours")
        .metavar("0/1")
        .default_value(ENABLE_INFO_RECT)
        .scan<'i', int>();
    options_info.add_argument("-emm", "--enable_minimap")
        .help("enable minimap")
        .metavar("0/1")
        .default_value(ENABLE_MINIMAP)
        .scan<'i', int>();
    options_info.add_argument("-emf", "--enable_minimap_fullscreen")
        .help("enable minimap fullscreen")
        .metavar("0/1")
        .default_value(ENABLE_MINIMAP_FULLSCREEN)
        .scan<'i', int>();


    auto& options_ignore_and_alarm = program->add_group("Ignore & Alarm Options");
    options_ignore_and_alarm.add_argument("-iamake", "--ignore_alarm_make")
        .help("start creating ignore area and alarm pixels")
        .default_value(false)
        .implicit_value(true);

    auto& options_ignore = program->add_group("Ignore Options");
    options_ignore.add_argument("-eic", "--enable_ignore_contours")
        .help("enable ignoring contours/areas (specify with -ic)")
        .metavar("0/1")
        .default_value(ENABLE_IGNORE_CONTOURS)
        .scan<'i', int>();
    options_ignore.add_argument("-ic", "--ignore_contours")
        .help("specify ignore contours/areas (points seperated by space, contours seperated by comma) (e.g.: \"<x>x<y> ...,<x>x<y> ...\")")
        .metavar("\"<x>x<y> ...,<x>x<y> ...\"")
        .default_value(IGNORE_CONTOURS);
    options_ignore.add_argument("-icf", "--ignore_contours_file")
        .help("specify ignore contours/areas inside file (points seperated by space, contours seperated by new line) (e.g.: \"<x>x<y> ...\\n<x>x<y> ...\")")
        .metavar("ignore.txt")
        .default_value(IGNORE_CONTOURS_FILENAME);

    auto& options_alarm = program->add_group("Alarm Options");
    options_alarm.add_argument("-eap", "--enable_alarm_pixels")
        .help("enable alarm pixels (specify with -ap)")
        .metavar("0/1")
        .default_value(ENABLE_ALARM_PIXELS)
        .scan<'i', int>();
    options_alarm.add_argument("-ap", "--alarm_pixels")
        .help("specify alarm pixels (seperated by space) (e.g.: \"<x>x<y> <x>x<y> ...\")")
        .metavar("\"<x>x<y> <x>x<y> ...\"")
        .default_value(ALARM_PIXELS);
    options_alarm.add_argument("-apf", "--alarm_pixels_file")
        .help("specify alarm pixels inside file (seperated by new line) (e.g.: \"<x>x<y>\\n<x>x<y>...\")")
        .metavar("alarm.txt")
        .default_value(ALARM_PIXELS_FILE);


    auto& options_focus = program->add_group("Focus Channel Options");
    options_focus.add_argument("-fc", "--focus_channel")
        .help("special mode that focuses on single channel when detecting motion (don't load other channels)")
        .metavar("1-8")
        .default_value(FOCUS_CHANNEL)
        .scan<'i', int>();
    options_focus.add_argument("-fca", "--focus_channel_area")
        .help("specify motion area to zoom to (work with) (e.g.: \"<x>x<y> <w>x<h>\"")
        .metavar("\"<x>x<y> <w>x<h>\"")
        .default_value(FOCUS_CHANNEL_AREA);
    options_focus.add_argument("-fcs", "--focus_channel_sound")
        .help("make sound if motion is detected")
        .metavar("0/1")
        .default_value(FOCUS_CHANNEL_SOUND)
        .scan<'i', int>();

    auto& options_special = program->add_group("Special Options");
    options_special.add_argument("-lc", "--low_cpu")
        .help("low cpu mode (uses only channel 0 to draw everything)")
        .metavar("0/1")
        .default_value(LOW_CPU_MODE)
        .scan<'i', int>();
    options_special.add_argument("-lchqm", "--low_cpu_hq_motion")
        .help("if motion is detected get high quality after switching channel")
        .metavar("0/1")
        .default_value(LOW_CPU_MODE_HQ_MOTION)
        .scan<'i', int>();
    options_special.add_argument("-lchqmd", "--low_cpu_hq_motion_dual")
        .help("keep last 2 channels running in high quality (use this if motion is detected on 2 channels and it swaps them frequently)")
        .metavar("0/1")
        .default_value(LOW_CPU_MODE_HQ_MOTION_DUAL)
        .scan<'i', int>();

    return program;
}
