#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <unistd.h> // For getpid()
#include <vector>
#include <x86intrin.h> // For __rdtsc()

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif
#define UNUSED(x) (void)(x)
#define DP(x) D(printf("[DEBUG] %s", x))
#define DPL(x) D(printf("[DEBUG] %s\n", x))

#ifdef DEBUG_CPU
#define D_CPU(x) x
#else
#define D_CPU(x)
#endif

class CpuUsageMonitor {
  public:
    CpuUsageMonitor()
    {
        m_num_cores = std::thread::hardware_concurrency();
        m_thread = std::thread([this]() { cpu_usage_monitor(); });
    }

    ~CpuUsageMonitor()
    {
        m_running = false;
        m_thread.join();
    }

  private:
    double m_max_cpu = 0.0;
    std::thread m_thread;
    std::atomic<bool> m_running{true};
    pid_t m_pid = getpid(); // Store the process ID
    unsigned int m_num_cores;
    double m_min = std::numeric_limits<double>::max();
    double m_max = 0;
    double m_total = 0;
    double m_count = 0;

    std::vector<unsigned long long> get_process_cpu_stats()
    {
        std::ifstream file("/proc/" + std::to_string(m_pid) + "/stat");
        std::string line;
        std::vector<unsigned long long> process_stats;

        if (getline(file, line)) {
            std::stringstream ss(line);
            std::string temp;
            unsigned long long utime, stime, cutime, cstime;

            // Skip unwanted fields
            for (int i = 1; i <= 13; ++i) {
                ss >> temp;
            }

            ss >> utime >> stime >> cutime >> cstime;

            process_stats.push_back(utime);
            process_stats.push_back(stime);
            process_stats.push_back(cutime);
            process_stats.push_back(cstime);
        }

        return process_stats;
    }

    double calculate_process_cpu_usage(const std::vector<unsigned long long>& prev_stats, const std::vector<unsigned long long>& curr_stats, long long time_diff)
    {
        if (time_diff == 0) {
            return 0.0; // Avoid division by zero
        }
        unsigned long long prev_total = prev_stats[0] + prev_stats[1] + prev_stats[2] + prev_stats[3];
        unsigned long long curr_total = curr_stats[0] + curr_stats[1] + curr_stats[2] + curr_stats[3];

        unsigned long long total_diff = curr_total - prev_total;

        // Calculate the CPU usage as a percentage
        double cpu_usage = (static_cast<double>(total_diff) / sysconf(_SC_CLK_TCK) / time_diff) * 100.0;

        return cpu_usage;
    }

    void cpu_usage_monitor()
    {
        std::this_thread::sleep_for(std::chrono::seconds(15));

        std::vector<unsigned long long> prev_stats = get_process_cpu_stats();
        auto prev_time = std::chrono::steady_clock::now();

        while (m_running) {
            std::this_thread::sleep_for(std::chrono::seconds(10));

            auto curr_time = std::chrono::steady_clock::now();
            auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(curr_time - prev_time).count();

            std::vector<unsigned long long> curr_stats = get_process_cpu_stats();

            double cpu_usage = calculate_process_cpu_usage(prev_stats, curr_stats, time_diff);

            // Normalize CPU usage by the number of cores
            double normalized_cpu_usage = cpu_usage / m_num_cores;

            if (normalized_cpu_usage > m_max_cpu) {
                m_max_cpu = normalized_cpu_usage;
            }

            if (normalized_cpu_usage > m_max) m_max = normalized_cpu_usage;
            if (normalized_cpu_usage < m_min) m_min = normalized_cpu_usage;
            m_total += normalized_cpu_usage;
            m_count++;

            double avg = m_count > 0 ? m_total / m_count : 0;

            std::cout << "CPU Usage - Min: " << m_min
                      << " | Max: " << m_max
                      << " | Avg: " << avg << "\n";

            prev_stats = curr_stats;
            prev_time = curr_time;
        }
    }
};

class CpuTimerMs {

  public:
    CpuTimerMs(double& max) : m_max(max)
    {
        m_start = std::chrono::high_resolution_clock::now();
    }

    ~CpuTimerMs()
    {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - m_start;
        double e = elapsed.count();
        std::cout << "took: " << e << " ms\t" << m_max << "ms\n";
        if (e > m_max) m_max = e;
    }

  private:
    double& m_max;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

class CpuCyclesTimer {
  public:
    CpuCyclesTimer()
        : m_index(0), m_max(0), m_min(std::numeric_limits<unsigned long long>::max()),
          m_total(0), m_count(0), m_start(0), m_end(0) {}

    void start()
    {
        m_start = __rdtsc();
    }

    void stop()
    {
        m_index++;
        if (m_index < 600) { return; } // warm-up

        m_end = __rdtsc();
        auto elapsed = m_end - m_start;

        if (elapsed > m_max) m_max = elapsed;
        if (elapsed < m_min) m_min = elapsed;
        m_total += elapsed;
        m_count++;

        if (m_index % 300 == 0) {
            print();
        }
    }

    void print()
    {
        auto avg = m_count > 0 ? m_total / m_count : 0;
        std::cout << "CPU Cycles - Min: " << m_min
                  << " | Max: " << m_max
                  << " | Avg: " << avg << "\n";
    }

  private:
    unsigned long long m_index;
    unsigned long long m_max;
    unsigned long long m_min;
    unsigned long long m_total;
    unsigned long long m_count;
    unsigned long long m_start;
    unsigned long long m_end;
};
