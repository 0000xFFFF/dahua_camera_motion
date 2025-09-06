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

#define CAP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

// proj settings
#define DISPLAY_MODE_SINGLE 0
#define DISPLAY_MODE_ALL    1
#define DISPLAY_MODE_SORT   2
#define DISPLAY_MODE_KING   3
#define DISPLAY_MODE_TOP    4

#define USE_SUBTYPE1 false
#define W_0          704
#define H_0          576
#define W_HD         1920
#define H_HD         1080

#define MINIMAP_WIDTH  W_0/4
#define MINIMAP_HEIGHT H_0/4


// 1-6
#define CROP_WIDTH     704
#define CROP_HEIGHT    384

//
// ==[ build settings
//
#ifndef DEBUG
// #define DEBUG
#endif

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

#ifdef DEBUG
#ifndef ENABLE_INFO
#define ENABLE_INFO 1
#endif
#ifndef ENABLE_MINIMAP
#define ENABLE_MINIMAP 1
#endif
#endif


#ifndef CHANNEL_COUNT
#define CHANNEL_COUNT 8
#endif

#ifndef CURRENT_CHANNEL
#define CURRENT_CHANNEL 1
#endif

#ifndef ENABLE_INFO
#define ENABLE_INFO 0
#endif

#ifndef ENABLE_INFO_LINE
#define ENABLE_INFO_LINE 1
#endif

#ifndef ENABLE_INFO_RECT
#define ENABLE_INFO_RECT 1
#endif

#ifndef ENABLE_MINIMAP
#define ENABLE_MINIMAP 0
#endif

#ifndef ENABLE_MINIMAP_FULLSCREEN
#define ENABLE_MINIMAP_FULLSCREEN 0
#endif

#ifndef ENABLE_MOTION
#define ENABLE_MOTION 1
#endif

#ifndef MOTION_DETECT_AREA
#define MOTION_DETECT_AREA 10
#endif

#ifndef MOTION_DETECT_RECT_AREA
#define MOTION_DETECT_RECT_AREA 0
#endif

#ifndef MOTION_DETECT_MIN_MS
#define MOTION_DETECT_MIN_MS 1000
#endif

#ifndef MOTION_DETECT_LINGER_MS
#define MOTION_DETECT_LINGER_MS 3000 // after motion keep zoom for X ms
#endif

#ifndef ENABLE_MOTION_ZOOM_LARGEST
#define ENABLE_MOTION_ZOOM_LARGEST 0
#endif

#ifndef ENABLE_IGNORE_CONTOURS
#define ENABLE_IGNORE_CONTOURS 1
#endif

#ifndef IGNORE_CONTOURS
#define IGNORE_CONTOURS ""
#endif

#ifndef IGNORE_CONTOURS_FILENAME
#define IGNORE_CONTOURS_FILENAME ""
#endif

#ifndef ENABLE_ALARM_PIXELS
#define ENABLE_ALARM_PIXELS 1
#endif

#ifndef ALARM_PIXELS
#define ALARM_PIXELS ""
#endif

#ifndef ALARM_PIXELS_FILE
#define ALARM_PIXELS_FILE ""
#endif

#ifndef USE_CUDA
#define USE_CUDA 0
#endif

#define KING_LAYOUT_REL  1
#define KING_LAYOUT_CIRC 2

#ifndef KING_LAYOUT
#define KING_LAYOUT 1
#endif

#ifndef DISPLAY_MODE
#define DISPLAY_MODE DISPLAY_MODE_SINGLE
#endif

#ifndef ENABLE_TOUR
#define ENABLE_TOUR 0
#endif

#ifndef ENABLE_FULLSCREEN_CHANNEL
#define ENABLE_FULLSCREEN_CHANNEL 0
#endif

// milis
// #define SLEEP_MS_MOTION 300
// #define SLEEP_MS_DRAW 200
// #define SLEEP_MS_ERROR 10
// #define SLEEP_MS_FRAME 50 // frame reader sleep

#ifndef TOUR_MS
#define TOUR_MS 3000
#endif

#ifndef CONN_RETRY_MS
#define CONN_RETRY_MS 10000
#endif

#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH (int)(W_HD*0.8)
#endif

#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT (int)(H_HD*0.8)
#endif

#ifndef NO_RESIZE
#define NO_RESIZE 0
#endif

#ifndef DEFAULT_WINDOW_NAME
#define DEFAULT_WINDOW_NAME "Motion"
#endif

#ifndef FOCUS_CHANNEL
#define FOCUS_CHANNEL -1
#endif

#ifndef FOCUS_CHANNEL_AREA
#define FOCUS_CHANNEL_AREA ""
#endif

#ifndef FOCUS_CHANNEL_SOUND
#define FOCUS_CHANNEL_SOUND 0
#endif

// #define SLEEP_MS_FRAME 300
