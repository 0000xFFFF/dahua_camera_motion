GCC = g++
FILES = *.cpp
RELEASE_ARGS = -g -Wall -Wextra -O2
LIBS = `pkg-config --cflags --libs opencv4`
OUTPUT = dcm_master

debug:
	$(GCC) $(FILES) $(LIBS) -o $(OUTPUT)

release:
	$(GCC) $(RELEASE_ARGS) $(LIBS) $(FILES) -o $(OUTPUT)
