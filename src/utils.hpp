#pragma once

#include "opencv2/core/types.hpp"
#include <SDL2/SDL_mixer.h>
#include <string>
#include <vector>

void set_thread_affinity(int core_id);
std::string exec(const char* cmd);
std::pair<int, int> detect_screen_size(const int& index);
void play_unique_sound(Mix_Chunk* sound);
void print_contour(const std::vector<cv::Point>& contour);
std::string bool_to_str(bool b);
