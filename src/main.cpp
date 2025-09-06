#include <argparse/argparse.hpp>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <sched.h>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "debug.hpp"
#include "globals.hpp"
#include "motion_detector.hpp"
#include "utils.hpp"

#ifdef DEBUG_CPU
#include "debug.hpp"
#endif

std::unique_ptr<MotionDetector> motionDetector;

// char g_sfxp_8bit_clicky[] = "sfx/clicky-8-bit-sfx.wav";
Mix_Chunk* g_sfx_8bit_clicky;

#include "sfx.h"

Mix_Chunk* load_embedded_sound()
{
    SDL_RWops* rw = SDL_RWFromConstMem(sfx_clicky_8_bit_sfx_wav, sfx_clicky_8_bit_sfx_wav_len);
    if (!rw) {
        SDL_Log("SDL_RWFromConstMem failed: %s", SDL_GetError());
        return NULL;
    }

    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1); // 1 = free RWops automatically
    if (!chunk) {
        SDL_Log("Mix_LoadWAV_RW failed: %s", Mix_GetError());
    }

    return chunk;
}

int main(int argc, char* argv[])
{
    // Add signal handling
    std::signal(SIGINT, [](int) {
        motionDetector->stop();
        motionDetector.release();
        std::cout << "SIGINT" << std::endl;
        std::exit(0);
    });

#ifdef DEBUG_CPU
    CpuUsageMonitor monitor;
#endif

    argparse::ArgumentParser program("dcm_master");
    program.add_description("motion detection kiosk for dahua cameras");

    auto& options_required = program.add_group("Required Options");
    options_required.add_argument("-i", "--ip")
        .help("ip to connect to")
        .required();
    options_required.add_argument("-u", "--username")
        .help("account username")
        .required();
    options_required.add_argument("-p", "--password")
        .help("account password")
        .required();

    auto& options_window = program.add_group("Window Options");
    options_window.add_argument("-ww", "--width")
        .help("window width")
        .default_value(DEFAULT_WIDTH)
        .scan<'i', int>();
    options_window.add_argument("-wh", "--height")
        .help("window height")
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
        .default_value(0)
        .scan<'i', int>();

    auto& options_start = program.add_group("Start Options");
    options_start.add_argument("-dm", "--display_mode")
        .help("display mode for cameras (0 = single, 1 = all, 2 = sort, 3 = king, 4 = top")
        .metavar("0-4")
        .default_value(DISPLAY_MODE)
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

    auto& options_motion = program.add_group("Motion Detection Options");
    options_motion.add_argument("-em", "--enable_motion")
        .help("enable motion")
        .metavar("0/1")
        .default_value(ENABLE_MOTION)
        .scan<'i', int>();
    options_motion.add_argument("-a", "--area")
        .help("min contour area for motion detection")
        .default_value(MOTION_DETECT_AREA)
        .scan<'i', int>();
    options_motion.add_argument("-ra", "--rarea")
        .help("min contour's bounding rectangle area for detection")
        .default_value(MOTION_DETECT_RECT_AREA)
        .scan<'i', int>();
    options_motion.add_argument("-ms", "--motion_detect_min_ms")
        .help("minimum milliseconds of detected motion to switch channel")
        .default_value(MOTION_DETECT_MIN_MS)
        .scan<'i', int>();
    options_motion.add_argument("-emzl", "--enable_motion_zoom_largest")
        .help("zoom channel on largest detected motion")
        .metavar("0/1")
        .default_value(ENABLE_MOTION_ZOOM_LARGEST)
        .scan<'i', int>();

    auto& options_tour = program.add_group("Tour Options");
    options_tour.add_argument("-et", "--enable_tour")
        .help("tour, switch channels every X ms (set with -tms)")
        .metavar("0/1")
        .default_value(ENABLE_TOUR)
        .scan<'i', int>();
    options_tour.add_argument("-tms", "--tour_ms")
        .help("how many ms to focus on a channel before switching")
        .metavar("0/1")
        .default_value(TOUR_MS)
        .scan<'i', int>();

    auto& options_info = program.add_group("Info Options");
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
        .help("enable drawing largest motion rectangle")
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

    auto& options_ignore = program.add_group("Ignore Options");
    options_ignore.add_argument("-eic", "--enable_ignore_contours")
        .help("enable ignoring contours/areas (specify with -ic)")
        .metavar("0/1")
        .default_value(ENABLE_IGNORE_CONTOURS)
        .scan<'i', int>();
    options_ignore.add_argument("-ic", "--ignore_contours")
        .help("specify ignore contours/areas (e.g.: <x>x<y>,...;<x>x<y>,...)")
        .default_value(IGNORE_CONTOURS);
    options_ignore.add_argument("-icf", "--ignore_contours_file")
        .help("specify ignore contours/areas inside file (seperated by new line) (e.g.: \"<x>x<y>,...\\n<x>x<y>,...\")")
        .default_value(IGNORE_CONTOURS_FILENAME);

    auto& options_alarm = program.add_group("Alarm Options");
    options_alarm.add_argument("-eap", "--enable_alarm_pixels")
        .help("enable alarm pixels (specify with -ap)")
        .metavar("0/1")
        .default_value(ENABLE_ALARM_PIXELS)
        .scan<'i', int>();
    options_alarm.add_argument("-ap", "--alarm_pixels")
        .help("specify alarm pixels (e.g.: <x>x<y>;<x>x<y>;...)")
        .default_value(ALARM_PIXELS);
    options_alarm.add_argument("-apf", "--alarm_pixels_file")
        .help("specify alarm pixels inside file (seperated by new line) (e.g.: \"<x>x<y>\\n<x>x<y>...\")")
        .default_value(ALARM_PIXELS_FILE);


    auto& options_focus = program.add_group("Focus Channel Options");
    options_focus.add_argument("-fc", "--focus_channel")
        .help("special mode that focuses on single channel when detecting motion (don't load other channels)")
        .default_value(FOCUS_CHANNEL)
        .scan<'i', int>();
    options_focus.add_argument("-fca", "--focus_channel_area")
        .help("specify motion area to zoom to (work with) (e.g.: \"<x>x<y>;<w>x<h>\"")
        .metavar("<x>x<y>;<w>x<h>")
        .default_value(FOCUS_CHANNEL_AREA);
    options_focus.add_argument("-fcs", "--focus_channel_sound")
        .help("make sound if motion is detected")
        .default_value(FOCUS_CHANNEL_SOUND)
        .scan<'i', int>();

    // init sdl for playing sounds
    if (SDL_Init(SDL_INIT_AUDIO) < 0) return 1;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return 1;

    // g_sfx_8bit_clicky = Mix_LoadWAV(g_sfxp_8bit_clicky);
    // if (!g_sfx_8bit_clicky) {
    //     std::cout << "can't load sfx: " << g_sfxp_8bit_clicky << std::endl;
    //     return 1;
    // }
    g_sfx_8bit_clicky = load_embedded_sound();

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
                program.get<int>("tour_ms"),
                program.get<int>("enable_info"),
                program.get<int>("enable_info_line"),
                program.get<int>("enable_info_rect"),
                program.get<int>("enable_minimap"),
                program.get<int>("enable_minimap_fullscreen"),
                program.get<int>("enable_fullscreen_channel"),
                program.get<int>("enable_ignore_contours"),
                program.get<int>("enable_alarm_pixels"),
                program.get<std::string>("ignore_contours"),
                program.get<std::string>("ignore_contours_file"),
                program.get<std::string>("alarm_pixels"),
                program.get<std::string>("alarm_pixels_file"),
                program.get<int>("focus_channel"),
                program.get<std::string>("focus_channel_area"),
                program.get<int>("focus_channel_sound")

            );

            motionDetector->draw_loop();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    Mix_PlayChannel(-1, g_sfx_8bit_clicky, 0);

    Mix_FreeChunk(g_sfx_8bit_clicky);
    Mix_CloseAudio();
    SDL_Quit();

    DPL("main return 0");
    return 0;
}
