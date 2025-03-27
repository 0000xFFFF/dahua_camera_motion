GCC = clang++
FILES = src/*.cpp
ARGS = -fcolor-diagnostics -O3
DEBUG_ARGS = -D DEBUG -g -fno-omit-frame-pointer
RELEASE_ARGS = -Wall -Wextra -s -march=native
LIBS = `pkg-config --cflags --libs opencv4`
EXEC = dcm_master

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

debug:
	$(GCC) $(ARGS) $(DEBUG_ARGS) $(FILES) $(LIBS) -o $(EXEC)

debugv:
	$(GCC) $(ARGS) $(DEBUG_ARGS) -D DEBUG_VERBOSE $(FILES) $(LIBS) -o $(EXEC)

cputest:
	$(GCC) $(ARGS) -D DEBUG -D DEBUG_VERBOSE $(RELEASE_ARGS) $(FILES) $(LIBS) -o $(EXEC)

fast:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D CPU_HIGH_FAST $(LIBS) $(FILES) -o $(EXEC)

tour:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D TOUR $(LIBS) $(FILES) -o $(EXEC)

touronly:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D TOUR_ONLY $(LIBS) $(FILES) -o $(EXEC)

king:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D KING $(LIBS) $(FILES) -o $(EXEC)

top:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D TOP $(LIBS) $(FILES) -o $(EXEC)

tourtop:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D TOUR_TOP $(LIBS) $(FILES) -o $(EXEC)

touronlytop:
	$(GCC) $(ARGS) $(RELEASE_ARGS) -D TOUR_ONLY_TOP $(LIBS) $(FILES) -o $(EXEC)

install:
	install -m 755 $(EXEC) $(BINDIR)

uninstall:
	rm -f $(BINDIR)/$(EXEC)

release:
	$(GCC) $(ARGS) $(RELEASE_ARGS) $(LIBS) $(FILES) -o $(EXEC)

