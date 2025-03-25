#pragma once

#define ENABLE_INFO 0
#define ENABLE_MINIMAP 0
#define ENABLE_MINIMAP_FULLSCREEN 0
#define ENABLE_MOTION 1
#define MOTION_DETECT_AREA 10
#define MOTION_DETECT_MIN_FRAMES 10
#define MOTION_DISPLAY_MODE 1
#define MOTION_DISPLAY_MODE_NONE 0
#define MOTION_DISPLAY_MODE_LARGEST 1
#define MOTION_DISPLAY_MODE_SORT 2
#define MOTION_DISPLAY_MODE_MULTI 3
#define MOTION_DISPLAY_MODE_KING 4
#define MOTION_DISPLAY_MODE_TOP 5
#define ENABLE_TOUR 0
#define ENABLE_FULLSCREEN_CHANNEL 0
#define TOUR_SLEEP_MS 3000
#define DRAW_SLEEP_MS 100
#define ERROR_SLEEP_MS 10

#define TOUR_FRAME_COUNT (TOUR_SLEEP_MS/DRAW_SLEEP_MS)

#ifdef DEBUG
#undef ENABLE_INFO
#define ENABLE_INFO 1
#undef ENABLE_MINIMAP
#define ENABLE_MINIMAP 1
//#undef DRAW_SLEEP_MS
//#define DRAW_SLEEP_MS 10
#endif

#ifdef CPU_LOW_SLOW
#undef DRAW_SLEEP_MS
#define DRAW_SLEEP_MS 300
#undef MOTION_DETECT_MIN_FRAMES
#define MOTION_DETECT_MIN_FRAMES 1
#endif

#ifdef CPU_HIGH_FAST
#undef DRAW_SLEEP_MS
#define DRAW_SLEEP_MS 1
#endif

#ifdef TOUR
#undef ENABLE_TOUR
#define ENABLE_TOUR 1
#endif

#ifdef TOUR_ONLY
#undef ENABLE_TOUR
#define ENABLE_TOUR 1
#undef ENABLE_MOTION
#define ENABLE_MOTION 0
#endif

#ifdef MULTI
#undef MOTION_DISPLAY_MODE
#define MOTION_DISPLAY_MODE MOTION_DISPLAY_MODE_MULTI
#endif

#ifdef KING
#undef MOTION_DISPLAY_MODE
#define MOTION_DISPLAY_MODE MOTION_DISPLAY_MODE_KING
#endif

#ifdef TOP
#undef MOTION_DISPLAY_MODE
#define MOTION_DISPLAY_MODE MOTION_DISPLAY_MODE_TOP
#endif

#ifdef TOUR_TOP
#undef ENABLE_TOUR
#define ENABLE_TOUR 1
#undef MOTION_DISPLAY_MODE
#define MOTION_DISPLAY_MODE MOTION_DISPLAY_MODE_TOP
#endif

#ifdef TOUR_ONLY_TOP
#undef ENABLE_TOUR
#define ENABLE_TOUR 1
#undef ENABLE_MOTION
#define ENABLE_MOTION 0
#undef MOTION_DISPLAY_MODE
#define MOTION_DISPLAY_MODE MOTION_DISPLAY_MODE_TOP
#endif


#define USE_SUBTYPE1 false
#define W_0 704
#define H_0 576
#define W_HD 1920
#define H_HD 1080
