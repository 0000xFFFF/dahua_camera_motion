GCC = g++
FILES = *.cpp
DEBUG_ARGS = -D DEBUG -g
RELEASE_ARGS = -Wall -Wextra -O2 -s
LIBS = `pkg-config --cflags --libs opencv4`
OUTPUT = dcm_master

debug:
	$(GCC) $(DEBUG_ARGS) $(FILES) $(LIBS) -o $(OUTPUT)

fast:
	$(GCC) $(RELEASE_ARGS) -D FAST $(LIBS) $(FILES) -o $(OUTPUT)

slow:
	$(GCC) $(RELEASE_ARGS) -D LOWCPU $(LIBS) $(FILES) -o $(OUTPUT)

release:
	$(GCC) $(RELEASE_ARGS) $(LIBS) $(FILES) -o $(OUTPUT)
