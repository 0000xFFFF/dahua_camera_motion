#pragma once

#include <SDL2/SDL_mixer.h>
#include <string>

void set_thread_affinity(int core_id);
std::string exec(const char* cmd);
std::pair<int, int> detect_screen_size(const int& index);
void play_unique_sound(Mix_Chunk* sound);
