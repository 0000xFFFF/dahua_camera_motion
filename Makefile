CC = clang++
FILES = src/*.cpp
ARGS = 
DEBUG_ARGS = -DDEBUG -g -fno-omit-frame-pointer
RELEASE_ARGS = -Wall -Wextra -s -march=native -O3
LIBS = `pkg-config --cflags --libs opencv4 sdl2` -lSDL2_mixer -lavformat -lavcodec -lavutil -lswscale -lavdevice
EXEC = dcm_master

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

debug:
	$(CC) $(ARGS) $(DEBUG_ARGS) $(FILES) $(LIBS) -o $(EXEC)

debug_fps:
	$(CC) $(ARGS) $(DEBUG_ARGS) -DDEBUG_FPS $(FILES) $(LIBS) -o $(EXEC)

bench_cpu:
	$(CC) $(ARGS) $(DEBUG_ARGS) -DDEBUG_CPU $(FILES) $(LIBS) -o $(EXEC)

music:
	xxd -i sfx/clicky-8-bit-sfx.wav > src/sfx.h

install:
	install -m 755 $(EXEC) $(BINDIR)

uninstall:
	rm $(BINDIR)/$(EXEC)

lowcpu:
	$(CC) $(ARGS) $(RELEASE_ARGS) -DSLEEP_MS_DRAW=100 -DSLEEP_MS_MOTION=100 $(LIBS) $(FILES) -o $(EXEC)


release:
	$(CC) $(ARGS) $(RELEASE_ARGS) $(LIBS) $(FILES) -o $(EXEC)
