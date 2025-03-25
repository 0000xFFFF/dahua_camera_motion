#pragma once

#define ENABLE_INFO 0
#define ENABLE_MINIMAP 0
#define ENABLE_MINIMAP_FULLSCREEN 0
#define ENABLE_MOTION 1
#define MOTION_DETECT_AREA 10
#define MOTION_DETECT_MIN_FRAMES 10
#define MOTION_DISPLAY_MODE 1
#define MOTION_DISPLAY_MODE_NONE 0
#define MOTION_DISPLAY_MODE_LARGEST_ONLY 1
#define MOTION_DISPLAY_MODE_SORT_BY_AREA_ALL 2
#define MOTION_DISPLAY_MODE_SORT_BY_AREA_MOTION 3
#define MOTION_DISPLAY_MODE_KING 4
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
#define MOTION_DISPLAY_MODE MOTION_DISPLAY_MODE_SORT_BY_AREA_MOTION
#endif



#define USE_SUBTYPE1 false
#define W_0 704
#define H_0 576
#define W_HD 1920
#define H_HD 1080
