#include "sound.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

// char g_sfxp_8bit_clicky[] = "sfx/clicky-8-bit-sfx.wav";
#include "sfx.h"

Mix_Chunk* g_sfx_8bit_clicky;
Mix_Chunk* load_embedded_sound()
{
    SDL_RWops* rw = SDL_RWFromConstMem(sfx_clicky_8_bit_sfx_wav, sfx_clicky_8_bit_sfx_wav_len);
    if (!rw) {
        SDL_Log("SDL_RWFromConstMem failed: %s", SDL_GetError());
        return NULL;
    }

    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1); // 1 = free RWops automatically
    if (!chunk) {
        SDL_Log("Mix_LoadWAV_RW failed: %s", Mix_GetError());
    }

    return chunk;
}

int init_sound() {

    // init sdl for playing sounds
    if (SDL_Init(SDL_INIT_AUDIO) < 0) return 1;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return 1;

    // g_sfx_8bit_clicky = Mix_LoadWAV(g_sfxp_8bit_clicky);
    // if (!g_sfx_8bit_clicky) {
    //     std::cout << "can't load sfx: " << g_sfxp_8bit_clicky << std::endl;
    //     return 1;
    // }
    g_sfx_8bit_clicky = load_embedded_sound();

    return 0;
}

int uninit_sound() {

    Mix_PlayChannel(-1, g_sfx_8bit_clicky, 0);
    Mix_FreeChunk(g_sfx_8bit_clicky);
    Mix_CloseAudio();
    SDL_Quit();

    return 0;
}
