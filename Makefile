GCC = clang++
FILES = src/*.cpp
ARGS = -O3
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

#
# ==[ TESTING CPU
#

test_0:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_CPU -D ENABLE_MINIMAP_FULLSCREEN=1 $(LIBS) $(FILES) -o $(EXEC)

debug_cpu:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_CPU $(FILES) $(LIBS) -o $(EXEC)

test_all:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_CPU -D DISPLAY_MODE=DISPLAY_MODE_ALL -D ENABLE_MOTION=0 $(LIBS) $(FILES) -o $(EXEC)

test_sort:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_CPU -D DISPLAY_MODE=DISPLAY_MODE_SORT -D ENABLE_MOTION=0 $(LIBS) $(FILES) -o $(EXEC)

test_king:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_CPU -D DISPLAY_MODE=DISPLAY_MODE_KING -D ENABLE_MOTION=0 $(LIBS) $(FILES) -o $(EXEC)

test_top:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_CPU -D DISPLAY_MODE=DISPLAY_MODE_TOP -D ENABLE_MOTION=0 $(LIBS) $(FILES) -o $(EXEC)


#
# ==[ funky opts
#
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
# ==[ layout options
#
single:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_SINGLE $(LIBS) $(FILES) -o $(EXEC)

all:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_ALL $(LIBS) $(FILES) -o $(EXEC)

lall:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_ALL -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

sort:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_ALL $(LIBS) $(FILES) -o $(EXEC)

lsort:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_ALL -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

king:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING $(LIBS) $(FILES) -o $(EXEC)

kingcirc:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 $(LIBS) $(FILES) -o $(EXEC)

lking:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

top:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_TOP $(LIBS) $(FILES) -o $(EXEC)

ltop:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_TOP -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

ttop:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 DISPLAY_MODE=DISPLAY_MODE_TOP $(LIBS) $(FILES) -o $(EXEC)

totop:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D ENABLE_TOUR=1 -D ENABLE_MOTION=0 -D DISPLAY_MODE=DISPLAY_MODE_TOP $(LIBS) $(FILES) -o $(EXEC)

#
# ==[ install / uninstall
#
install:
	install -m 755 $(EXEC) $(BINDIR)

uninstall:
	rm -f $(BINDIR)/$(EXEC)

#
# ==[ releases
#

# king, circ, motion slow, draw fast, tour
r1:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 -D SLEEP_MS_MOTION=300 -D SLEEP_MS_DRAW=80 -D ENABLE_TOUR=1 $(LIBS) $(FILES) -o $(EXEC)

# king, circ, motion slow, draw fast, tour, zoom
r2:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 -D SLEEP_MS_MOTION=300 -D SLEEP_MS_DRAW=80 -D ENABLE_TOUR=1 -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

r2_100:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 -D SLEEP_MS_MOTION=300 -D SLEEP_MS_DRAW=100 -D ENABLE_TOUR=1 -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

r2_150:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_KING -D KING_LAYOUT=2 -D SLEEP_MS_MOTION=300 -D SLEEP_MS_DRAW=150 -D ENABLE_TOUR=1 -D ENABLE_MOTION_ZOOM_LARGEST=1 $(LIBS) $(FILES) -o $(EXEC)

# single, motion slow, draw fast, tour
r3:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D DISPLAY_MODE=DISPLAY_MODE_SINGLE -D SLEEP_MS_MOTION=300 -D SLEEP_MS_DRAW=50 -D ENABLE_TOUR=1 $(LIBS) $(FILES) -o $(EXEC)

release: r2
	

