#pragma once

#include <string>

void set_thread_affinity(int core_id);
std::string exec(const char* cmd);
std::pair<int, int> detect_screen_size(const int& index);
