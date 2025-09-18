#pragma once

#include <SDL2/SDL_mixer.h>

extern Mix_Chunk* g_sfx_8bit_clicky;
int init_sound();
int uninit_sound();
