# dahua_camera_motion
motion detection kiosk for dahua cameras

# Requirements
* make
* clang++
* argparse
* OpenCV (libopencv)
* sdl2
* sdl2_mixer

# Build & Install
```sh
make release
sudo make install
```

# Example
```sh
./dcm_master -i <ip> -u <user> -p <password> -fs
```

# Usage
```sh
./dcm_master --help
```
```
Usage: dcm_master [--help] [--version] --ip VAR --username VAR --password VAR [--width VAR] [--height VAR] [--fullscreen] [--detect] [--resolution VAR] [--display_mode 0-4] [--current_channel 1-8] [--enable_fullscreen_channel 0/1] [--enable_motion 0/1] [--area VAR] [--rarea VAR] [--motion_detect_min_ms VAR] [--enable_motion_zoom_largest 0/1] [--enable_tour 0/1] [--tour_ms 0/1] [--enable_info 0/1] [--enable_info_line 0/1] [--enable_info_rect 0/1] [--enable_minimap 0/1] [--enable_minimap_fullscreen 0/1] [--enable_ignore_contours 0/1] [--ignore_contours VAR] [--ignore_contours_file VAR] [--enable_alarm_pixels 0/1] [--alarm_pixels VAR] [--alarm_pixels_file VAR] [--focus_channel VAR] [--focus_channel_area <x>x<y>;<w>x<h>] [--focus_channel_sound VAR]

motion detection kiosk for dahua cameras

Optional arguments:
  -h, --help                           shows help message and exits 
  -v, --version                        prints version information and exits 

Required Options (detailed usage):
  -i, --ip                             ip to connect to [required]
  -u, --username                       account username [required]
  -p, --password                       account password [required]

Window Options (detailed usage):
  -ww, --width                         window width [nargs=0..1] [default: 1920]
  -wh, --height                        window height [nargs=0..1] [default: 1080]
  -fs, --fullscreen                    start in fullscreen mode 
  -d, --detect                         detect screen size with xrandr 
  -r, --resolution                     index of resolution to use [nargs=0..1] [default: 0]

Start Options (detailed usage):
  -dm, --display_mode                  display mode for cameras (0 = single, 1 = all, 2 = sort, 3 = king, 4 = top [nargs=0..1] [default: 0]
  -ch, --current_channel               which channel to start with [nargs=0..1] [default: 1]
  -efc, --enable_fullscreen_channel    enable fullscreen channel [nargs=0..1] [default: 0]

Motion Detection Options (detailed usage):
  -em, --enable_motion                 enable motion [nargs=0..1] [default: 1]
  -a, --area                           min contour area for motion detection [nargs=0..1] [default: 10]
  -ra, --rarea                         min contour's bounding rectangle area for detection [nargs=0..1] [default: 0]
  -ms, --motion_detect_min_ms          minimum milliseconds of detected motion to switch channel [nargs=0..1] [default: 1000]
  -emzl, --enable_motion_zoom_largest  zoom channel on largest detected motion [nargs=0..1] [default: 0]

Tour Options (detailed usage):
  -et, --enable_tour                   tour, switch channels every X ms (set with -tms) [nargs=0..1] [default: 0]
  -tms, --tour_ms                      how many ms to focus on a channel before switching [nargs=0..1] [default: 3000]

Info Options (detailed usage):
  -ei, --enable_info                   enable drawing info [nargs=0..1] [default: 1]
  -eil, --enable_info_line             enable drawing line info (motion, linger, tour, ...) [nargs=0..1] [default: 1]
  -eir, --enable_info_rect             enable drawing largest motion rectangle [nargs=0..1] [default: 1]
  -emm, --enable_minimap               enable minimap [nargs=0..1] [default: 1]
  -emf, --enable_minimap_fullscreen    enable minimap fullscreen [nargs=0..1] [default: 0]

Ignore Options (detailed usage):
  -eic, --enable_ignore_contours       enable ignoring contours/areas (specify with -ic) [nargs=0..1] [default: 1]
  -ic, --ignore_contours               specify ignore contours/areas (e.g.: <x>x<y>,...;<x>x<y>,...) [nargs=0..1] [default: ""]
  -icf, --ignore_contours_file         specify ignore contours/areas inside file (seperated by new line) (e.g.: "<x>x<y>,...\n<x>x<y>,...") [nargs=0..1] [default: ""]

Alarm Options (detailed usage):
  -eap, --enable_alarm_pixels          enable alarm pixels (specify with -ap) [nargs=0..1] [default: 1]
  -ap, --alarm_pixels                  specify alarm pixels (e.g.: <x>x<y>;<x>x<y>;...) [nargs=0..1] [default: ""]
  -apf, --alarm_pixels_file            specify alarm pixels inside file (seperated by new line) (e.g.: "<x>x<y>\n<x>x<y>...") [nargs=0..1] [default: ""]

Focus Channel Options (detailed usage):
  -fc, --focus_channel                 special mode that focuses on single channel when detecting motion (don't load other channels) [nargs=0..1] [default: -1]
  -fca, --focus_channel_area           specify motion area to zoom to (work with) (e.g.: "<x>x<y>;<w>x<h>" [nargs=0..1] [default: ""]
  -fcs, --focus_channel_sound          make sound if motion is detected [nargs=0..1] [default: 0]
```
