GCC = g++
FILES = *.cpp
DEBUG_ARGS = -D DEBUG -g
RELEASE_ARGS = -Wall -Wextra -O2
LIBS = `pkg-config --cflags --libs opencv4`
OUTPUT = dcm_master

debug:
	$(GCC) $(DEBUG_ARGS) $(FILES) $(LIBS) -o $(OUTPUT)

release:
	$(GCC) $(RELEASE_ARGS) $(LIBS) $(FILES) -o $(OUTPUT)
