#pragma once

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

inline constexpr enum DISPLAY_MODE DISPLAY_MODE_DEFAULT = DISPLAY_MODE_SINGLE;

#define USE_SUBTYPE1 false
#define W_0          704
#define H_0          576
#define W_HD         1920
#define H_HD         1080

#define MINIMAP_WIDTH  W_0 / 4
#define MINIMAP_HEIGHT H_0 / 4

// 1-6
#define CROP_WIDTH  704
#define CROP_HEIGHT 384

#ifdef MAKE_IGNORE
#ifndef ENABLE_INFO
#define ENABLE_INFO 0
#endif
#ifndef ENABLE_MINIMAP
#define ENABLE_MINIMAP 0
#endif
#ifndef ENABLE_MINIMAP_FULLSCREEN
#define ENABLE_MINIMAP_FULLSCREEN 1
#endif
#ifndef ENABLE_IGNORE_CONTOURS
#define ENABLE_IGNORE_CONTOURS 1
#endif
#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH W_0
#endif
#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT H_0
#endif
#endif

// Channel configuration
inline constexpr int CHANNEL_COUNT = 8;
inline constexpr int CURRENT_CHANNEL = 1;

// Info overlay
inline constexpr bool ENABLE_INFO = false;
inline constexpr bool ENABLE_INFO_LINE = true;
inline constexpr bool ENABLE_INFO_RECT = true;

// Minimap
inline constexpr bool ENABLE_MINIMAP = false;
inline constexpr bool ENABLE_MINIMAP_FULLSCREEN = false;

// Motion detection
inline constexpr bool ENABLE_MOTION = true;
inline constexpr int MOTION_DETECT_AREA = 10;
inline constexpr int MOTION_DETECT_RECT_AREA = 0;
inline constexpr int MOTION_DETECT_MIN_MS = 1000;
inline constexpr int MOTION_DETECT_LINGER_MS = 3000; // after motion keep zoom for X ms
inline constexpr bool ENABLE_MOTION_ZOOM_LARGEST = false;

// Ignore contours
inline constexpr bool ENABLE_IGNORE_CONTOURS = true;
inline constexpr auto IGNORE_CONTOURS = "";
inline constexpr auto IGNORE_CONTOURS_FILENAME = "";

// Alarm pixels
inline constexpr bool ENABLE_ALARM_PIXELS = true;
inline constexpr auto ALARM_PIXELS = "";
inline constexpr auto ALARM_PIXELS_FILE = "";

// Hardware acceleration
inline constexpr bool USE_CUDA = false;

// Layout modes
inline constexpr int KING_LAYOUT_REL = 1;
inline constexpr int KING_LAYOUT_CIRC = 2;
inline constexpr int KING_LAYOUT = KING_LAYOUT_REL;

// Tour & fullscreen
inline constexpr bool ENABLE_TOUR = false;
inline constexpr bool ENABLE_FULLSCREEN_CHANNEL = false;
inline constexpr int TOUR_MS = 3000;

// Connection retries
inline constexpr int CONN_RETRY_MS = 10000;

// Window defaults
inline constexpr int DEFAULT_WIDTH = static_cast<int>(W_HD * 0.8);  // assumes W_HD is defined
inline constexpr int DEFAULT_HEIGHT = static_cast<int>(H_HD * 0.8); // assumes H_HD is defined
inline constexpr bool NO_RESIZE = false;
inline constexpr auto DEFAULT_WINDOW_NAME = "Motion";

// Focus channel
inline constexpr int FOCUS_CHANNEL = -1;
inline constexpr auto FOCUS_CHANNEL_AREA = "";
inline constexpr bool FOCUS_CHANNEL_SOUND = false;

// CPU mode
inline constexpr bool LOW_CPU_MODE = false;
