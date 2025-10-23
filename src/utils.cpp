#include "globals.hpp"
#include "opencv2/core/types.hpp"
#include <SDL2/SDL_mixer.h>
#include <array>
#include <iostream>
#include <sched.h>
#include <sstream>
#include <string>
#include <vector>

void set_thread_affinity(int core_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

// Helper function to execute shell commands
std::string exec(const char* cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Get screen resolution using xrandr
std::pair<int, int> detect_screen_size(const int& index)
{
    try {
        std::string xrandr_output = exec("xrandr | grep '*' | awk '{print $1}'");
        std::vector<std::string> resolutions;
        std::stringstream ss(xrandr_output);
        std::string res;
        std::cout << "detected resolutions:" << std::endl;
        while (std::getline(ss, res, '\n')) {
            std::cout << res << std::endl;
            resolutions.push_back(res);
        }
        const std::string& use = resolutions[index];
        std::cout << "using: " << use << std::endl;

        size_t x_pos = use.find('x');
        if (x_pos != std::string::npos) {
            int width = std::stoi(use.substr(0, x_pos));
            int height = std::stoi(use.substr(x_pos + 1));
            return {width, height};
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Warning: Failed to detect screen size: " << e.what() << std::endl;
    }
    return {W_HD, H_HD}; // Default fallback
}

void play_unique_sound(Mix_Chunk* sound)
{
    // Search if the same chunk is already playing
    int num_channels = Mix_AllocateChannels(-1);
    for (int i = 0; i < num_channels; i++) {
        if (Mix_GetChunk(i) == sound && Mix_Playing(i)) {
            return; // already playing, skip
        }
    }

    // Not playing yet, find a free channel and play
    Mix_PlayChannel(-1, sound, 0);
}

void print_contour(const std::vector<cv::Point>& contour)
{
    for (size_t x = 0; x < contour.size(); x++) {
        std::cout << contour[x].x << SPLIT_COORD << contour[x].y;
        if (x != contour.size() - 1) { std::cout << SPLIT_POINT; }
    }
}

std::string bool_to_str(bool b)
{
    return std::string(b ? "Yes" : "No");
}
