GCC = clang++
FILES = src/*.cpp
ARGS = -fcolor-diagnostics -O3
DEBUG_ARGS = -D DEBUG -g -fno-omit-frame-pointer
RELEASE_ARGS = -Wall -Wextra -s -march=native
LIBS = `pkg-config --cflags --libs opencv4` -fopenmp
EXEC = dcm_master

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

debug:
	$(GCC) $(ARGS) $(DEBUG_ARGS) $(FILES) $(LIBS) -o $(EXEC)

debug_fps:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_FPS $(FILES) $(LIBS) -o $(EXEC)

debug_cpu:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_CPU $(FILES) $(LIBS) -o $(EXEC)

tour:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 $(LIBS) $(FILES) -o $(EXEC)

# tour only
to:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 -D ENABLE_MOTION=0 $(LIBS) $(FILES) -o $(EXEC)

# tour only slow
tos:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 -D ENABLE_MOTION=0 -D SLEEP_MS_DRAW=100 $(LIBS) $(FILES) -o $(EXEC)

circ:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 -D ENABLE_MOTION=0 -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 $(LIBS) $(FILES) -o $(EXEC)

circfast:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 -D ENABLE_MOTION=0 -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 -D OVERRIDE_SLEEP=0 $(LIBS) $(FILES) -o $(EXEC)

#
# layout options
#
#
single:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_SINGLE $(LIBS) $(FILES) -o $(EXEC)

all:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_ALL $(LIBS) $(FILES) -o $(EXEC)

test_all:
	$(GCC) $(ARGS) -D DEBUG_CPU -D DISPLAY_MODE=DISPLAY_MODE_ALL -D ENABLE_MOTION=0 $(LIBS) $(FILES) -o $(EXEC)

# ALL + ZOOM LARGEST MOTION
lall:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_ALL -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

king:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING $(LIBS) $(FILES) -o $(EXEC)

kingcirc:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 $(LIBS) $(FILES) -o $(EXEC)

lking:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

top:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_TOP $(LIBS) $(FILES) -o $(EXEC)

test_top:
	$(GCC) $(ARGS) -D DEBUG_CPU -D DISPLAY_MODE=DISPLAY_MODE_TOP -D ENABLE_MOTION=0 $(LIBS) $(FILES) -o $(EXEC)

ltop:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_TOP -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

# tour top
ttop:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 DISPLAY_MODE=DISPLAY_MODE_TOP $(LIBS) $(FILES) -o $(EXEC)

# tour only top
totop:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 -D ENABLE_MOTION=0 -D DISPLAY_MODE=DISPLAY_MODE_TOP $(LIBS) $(FILES) -o $(EXEC)

install:
	install -m 755 $(EXEC) $(BINDIR)

uninstall:
	rm -f $(BINDIR)/$(EXEC)

# releases
# king, circ, motion slow, draw fast, tour
king_circ_ms_df_tour:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 -D SLEEP_MS_MOTION=300 -D SLEEP_MS_DRAW=80 -D ENABLE_TOUR=1 $(LIBS) $(FILES) -o $(EXEC)

# king, circ, motion slow, draw fast, tour, zoom
king_circ_ms_df_tour_zoom:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 -D SLEEP_MS_MOTION=300 -D SLEEP_MS_DRAW=80 -D ENABLE_TOUR=1 -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

# single, motion slow, draw fast, tour
single_ms_df_tour:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_SINGLE -D SLEEP_MS_MOTION=300 -D SLEEP_MS_DRAW=20 -D ENABLE_TOUR=1 $(LIBS) $(FILES) -o $(EXEC)


r1: king_circ_ms_df_tour
r2: king_circ_ms_df_tour_zoom
r3: single_ms_df_tour

release: r2
	

