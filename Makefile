GCC = g++
FILES = *.cpp
DEBUG_ARGS = -D DEBUG -g
RELEASE_ARGS = -Wall -Wextra -O2 -s
LIBS = `pkg-config --cflags --libs opencv4`
OUTPUT = dcm_master

debug:
	$(GCC) $(DEBUG_ARGS) $(FILES) $(LIBS) -o $(OUTPUT)

nodelay:
	$(GCC) $(RELEASE_ARGS) -D NODELAY $(LIBS) $(FILES) -o $(OUTPUT)

release:
	$(GCC) $(RELEASE_ARGS) $(LIBS) $(FILES) -o $(OUTPUT)
