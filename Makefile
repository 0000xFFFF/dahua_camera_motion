GCC = clang++
FILES = src/*.cpp
DEBUG_ARGS = -D DEBUG -g -O0 -fno-omit-frame-pointer
RELEASE_ARGS = -Wall -Wextra -s -O3 -march=native
LIBS = `pkg-config --cflags --libs opencv4`
OUTPUT = dcm_master

debug:
	$(GCC) $(DEBUG_ARGS) $(FILES) $(LIBS) -o $(OUTPUT)

debug_release:
	$(GCC) -D DEBUG $(RELEASE_ARGS) $(FILES) $(LIBS) -o $(OUTPUT)

fast:
	$(GCC) $(RELEASE_ARGS) -D CPU_HIGH_FAST $(LIBS) $(FILES) -o $(OUTPUT)

slow:
	$(GCC) $(RELEASE_ARGS) -D CPU_LOW_SLOW $(LIBS) $(FILES) -o $(OUTPUT)

tour:
	$(GCC) $(RELEASE_ARGS) -D TOUR $(LIBS) $(FILES) -o $(OUTPUT)

touronly:
	$(GCC) $(RELEASE_ARGS) -D TOUR_ONLY $(LIBS) $(FILES) -o $(OUTPUT)

debug_multi:
	$(GCC) $(DEBUG_ARGS) -D MULTI $(LIBS) $(FILES) -o $(OUTPUT)

multi:
	$(GCC) $(RELEASE_ARGS) -D MULTI $(LIBS) $(FILES) -o $(OUTPUT)

king:
	$(GCC) $(RELEASE_ARGS) -D KING $(LIBS) $(FILES) -o $(OUTPUT)

top:
	$(GCC) $(RELEASE_ARGS) -D TOP $(LIBS) $(FILES) -o $(OUTPUT)

tourtop:
	$(GCC) $(RELEASE_ARGS) -D TOUR_TOP $(LIBS) $(FILES) -o $(OUTPUT)

touronlytop:
	$(GCC) $(RELEASE_ARGS) -D TOUR_ONLY_TOP $(LIBS) $(FILES) -o $(OUTPUT)

release:
	$(GCC) $(RELEASE_ARGS) $(LIBS) $(FILES) -o $(OUTPUT)
