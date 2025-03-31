#pragma once

#ifndef CURRENT_CHANNEL
#define CURRENT_CHANNEL 1
#endif

#ifndef ENABLE_INFO
#define ENABLE_INFO 0
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
#define MOTION_DETECT_AREA 50
#endif

#ifndef MOTION_DETECT_RECT_AREA
#define MOTION_DETECT_RECT_AREA 50
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


#ifndef USE_CUDA
#define USE_CUDA 0
#endif

#define DISPLAY_MODE_SINGLE 0
#define DISPLAY_MODE_ALL 1
#define DISPLAY_MODE_SORT 2
#define DISPLAY_MODE_KING 3
#define DISPLAY_MODE_TOP 4

#define KING_LAYOUT_REL 1
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
// #define SLEEP_MS_DRAW 200           // idle
// #define SLEEP_MS_DRAW_DETECTED 65   // motion detected
// #define SLEEP_MS_DRAW_IDLE 200
// #define SLEEP_MS_ERROR 10
// #define SLEEP_MS_FRAME 50  // frame reader sleep

#ifndef TOUR_MS
#define TOUR_MS 3000
#endif

#ifdef DEBUG
#undef ENABLE_INFO
#define ENABLE_INFO 1
#undef ENABLE_MINIMAP
#define ENABLE_MINIMAP 1
#endif

#define USE_SUBTYPE1 false
#define W_0 704
#define H_0 576
#define W_HD 1920
#define H_HD 1080


#define SLEEP_MS_FRAME 10
