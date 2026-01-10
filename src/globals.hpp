#pragma once

#define VERSION "2.2.0"

//
// ==[ static useful stuff
//
#define KEY_BACKSPACE 8
#define KEY_ENTER     13
#define KEY_ESCAPE    27

#define KEY_WIN_ARROW_UP    2490368
#define KEY_WIN_ARROW_DOWN  2621440
#define KEY_WIN_ARROW_LEFT  2424832
#define KEY_WIN_ARROW_RIGHT 2555904
#define KEY_WIN_PAGE_UP     2162688
#define KEY_WIN_PAGE_DOWN   2228224
#define KEY_WIN_HOME        2359296
#define KEY_WIN_END         2293760

#define KEY_LINUX_ARROW_LEFT  81  // ←
#define KEY_LINUX_ARROW_RIGHT 83  // →
#define KEY_LINUX_ARROW_UP    82  // ↑
#define KEY_LINUX_ARROW_DOWN  84  // ↓
#define KEY_LINUX_PAGE_UP     104 // Depends on layout
#define KEY_LINUX_PAGE_DOWN   109
#define KEY_LINUX_HOME        106
#define KEY_LINUX_END         107

// proj settings
enum DISPLAY_MODE {
    DISPLAY_MODE_SINGLE,
    DISPLAY_MODE_ALL,
    DISPLAY_MODE_SORT,
    DISPLAY_MODE_KING,
    DISPLAY_MODE_TOP,
};

inline constexpr enum DISPLAY_MODE DISPLAY_MODE_DEFAULT = DISPLAY_MODE_KING;

inline constexpr int SUBTYPE = 0;
// Video Standards:
// Feature              PAL                                                      NTSC
// Full Name            Phase Alternating Line                                   National Television System Committee
// Region               Europe, Asia, Africa, Australia                          North America, Japan, parts of South America
// Frame Rate           25 fps (frames per second)                               30 fps (actually 29.97 fps)
// Power System Match   50 Hz power grid regions                                 60 Hz power grid regions
// Resolution (Analog)  720 × 576 pixels                                         720 × 480 pixels
// Color Encoding       Phase-alternating color signal (less color distortion)   Simpler color encoding (more color shift possible)
//
// we are using PAL here:
inline constexpr int W_0 = 704;
inline constexpr int H_0 = 576;
inline constexpr int W_HD = 1920;
inline constexpr int H_HD = 1080;

inline constexpr int MINIMAP_WIDTH = static_cast<int>(W_0 / 4);
inline constexpr int MINIMAP_HEIGHT = static_cast<int>(H_0 / 4);

// Channel configuration
inline constexpr int CHANNEL_COUNT = 8;
inline constexpr int CURRENT_CHANNEL = 1;

// Info overlay
inline constexpr int ENABLE_INFO = 0;
inline constexpr int ENABLE_INFO_LINE = 1;
inline constexpr int ENABLE_INFO_RECT = 1;

// Minimap
inline constexpr int ENABLE_MINIMAP = 0;
inline constexpr int ENABLE_MINIMAP_FULLSCREEN = 0;

// Motion detection
inline constexpr int ENABLE_MOTION = 1;
inline constexpr int MOTION_DETECT_AREA = 10;
inline constexpr int MOTION_DETECT_RECT_AREA = 0;
inline constexpr int MOTION_DETECT_MIN_MS = 1000;
inline constexpr int MOTION_DETECT_LINGER_MS = 3000; // after motion keep zoom for X ms
inline constexpr int ENABLE_MOTION_ZOOM_LARGEST = 1;

inline constexpr char SPLIT_COORD = 'x';
inline constexpr char SPLIT_POINT = ' ';
inline constexpr char SPLIT_LIST = ',';

// Ignore contours
inline constexpr int ENABLE_IGNORE_CONTOURS = 1;
inline constexpr auto IGNORE_CONTOURS = "";
inline constexpr auto IGNORE_CONTOURS_FILENAME = "";

// Alarm pixels
inline constexpr int ENABLE_ALARM_PIXELS = 1;
inline constexpr auto ALARM_PIXELS = "";
inline constexpr auto ALARM_PIXELS_FILE = "";

// Tour & fullscreen
inline constexpr int ENABLE_TOUR = 0;
inline constexpr int ENABLE_FULLSCREEN_CHANNEL = 0;
inline constexpr int TOUR_MS = 3000;

// Connection retries
inline constexpr int CONN_RETRY_MS = 10000;

// Window defaults
inline constexpr int DEFAULT_WIDTH = static_cast<int>(W_HD * 0.8);
inline constexpr int DEFAULT_HEIGHT = static_cast<int>(H_HD * 0.8);
inline constexpr bool NO_RESIZE = false;
inline constexpr auto DEFAULT_WINDOW_NAME = "Motion";

// Focus channel
inline constexpr int FOCUS_CHANNEL = -1;
inline constexpr auto FOCUS_CHANNEL_AREA = "";
inline constexpr int FOCUS_CHANNEL_SOUND = 0;

// low CPU mode
inline constexpr int LOW_CPU_MODE = 0;
inline constexpr int LOW_CPU_MODE_HQ_MOTION = 0;
inline constexpr int LOW_CPU_MODE_HQ_MOTION_DUAL = 0;

// #define USE_CUDA
