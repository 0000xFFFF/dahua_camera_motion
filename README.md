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
./dcm_master
```
```
Usage: dcm_master [--help] [--version] --ip VAR --username VAR --password VAR [--width VAR] [--height VAR] [--fullscreen] [--detect] [--resolution VAR] [--display_mode 0-4] [--area VAR] [--rarea VAR] [--motion_detect_min_ms VAR] [--enable_motion_zoom_largest 0/1] [--current_channel 1-6] [--enable_motion 0/1] [--enable_tour 0/1] [--enable_info 0/1] [--enable_info_line 0/1] [--enable_info_rect 0/1] [--enable_minimap 0/1] [--enable_minimap_fullscreen 0/1] [--enable_fullscreen_channel 0/1] [--enable_ignore_contours 0/1] [--ignore_contours VAR] [--ignore_contours_file VAR] [--enable_alarm_pixels 0/1] [--alarm_pixels VAR] [--alarm_pixels_file VAR]

Optional arguments:
  -h, --help                           shows help message and exits
  -v, --version                        prints version information and exits
  -i, --ip                             IP to connect to [required]
  -u, --username                       Account username [required]
  -p, --password                       Account password [required]
  -ww, --width                         Window width [nargs=0..1] [default: 1920]
  -wh, --height                        Window height [nargs=0..1] [default: 1080]
  -fs, --fullscreen                    Start in fullscreen mode
  -d, --detect                         Detect screen size with xrandr
  -r, --resolution                     index of resolution to use (default: 0) [nargs=0..1] [default: 0]
  -dm, --display_mode                  display mode for cameras (0 = single, 1 = all, 2 = sort, 3 = king, 4 = top [nargs=0..1] [default: 0]
  -a, --area                           Min contour area for detection [nargs=0..1] [default: 10]
  -ra, --rarea                         Min contour's bounding rectangle area for detection [nargs=0..1] [default: 0]
  -ms, --motion_detect_min_ms          min milliseconds of detected motion to switch channel [nargs=0..1] [default: 1000]
  -emzl, --enable_motion_zoom_largest  enable motion zoom largest [nargs=0..1] [default: 0]
  -ch, --current_channel               which channel to start with [nargs=0..1] [default: 1]
  -em, --enable_motion                 enable motion [nargs=0..1] [default: 1]
  -et, --enable_tour                   enable motion zoom largest [nargs=0..1] [default: 0]
  -ei, --enable_info                   enable drawing info [nargs=0..1] [default: 0]
  -eil, --enable_info_line             enable drawing line info (motion, linger, tour, ...) [nargs=0..1] [default: 1]
  -eir, --enable_info_rect             enable drawing motion rectangle [nargs=0..1] [default: 1]
  -emm, --enable_minimap               enable minimap [nargs=0..1] [default: 0]
  -emf, --enable_minimap_fullscreen    enable minimap fullscreen [nargs=0..1] [default: 0]
  -efc, --enable_fullscreen_channel    enable minimap fullscreen [nargs=0..1] [default: 0]
  -eic, --enable_ignore_contours       enable ignoring contours/areas (specify with -ic) [nargs=0..1] [default: 1]
  -ic, --ignore_contours               specify ignore contours/areas (e.g.: <x>x<y>,...;<x>x<y>,...) [nargs=0..1] [default: ""]
  -icf, --ignore_contours_file         specify ignore contours/areas inside file (seperated by new line) (e.g.: <x>x<y>,...\n<x>x<y>,...) [nargs=0..1] [default: ""]
  -eap, --enable_alarm_pixels          enable alarm pixels (specify with -ap) [nargs=0..1] [default: 1]
  -ap, --alarm_pixels                  specify alarm pixels (e.g.: <x>x<y>;<x>x<y>;...) [nargs=0..1] [default: ""]
  -apf, --alarm_pixels_file            specify alarm pixels inside file (seperated by new line) (e.g.: <x>x<y>\n<x>x<y>...) [nargs=0..1] [default: ""]
```
