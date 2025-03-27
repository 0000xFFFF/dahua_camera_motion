#pragma once

#include <iostream>
#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif
#define UNUSED(x) (void)(x)
#define DP(x) D(printf("[DEBUG] %s", x))
#define DPL(x) D(printf("[DEBUG] %s\n", x))

#ifdef DEBUG_VERBOSE
#include <fstream>

class CpuUsageMonitor {

  public:
    CpuUsageMonitor()
    {
        m_thread = std::thread([this]() { cpu_usage_monitor(); });
    }

    ~CpuUsageMonitor()
    {
        m_running = false;
        m_thread.join();
    }

  private:
    double m_max_cpu;
    std::thread m_thread;
    std::atomic<bool> m_running{true};

    std::vector<unsigned long long> get_cpu_stats()
    {
        std::ifstream file("/proc/stat");
        std::string line;
        std::vector<unsigned long long> cpu_stats;

        // Read the first line for the "cpu" stats
        if (getline(file, line)) {
            std::stringstream ss(line);
            std::string cpu;
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

            ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

            cpu_stats.push_back(user);
            cpu_stats.push_back(nice);
            cpu_stats.push_back(system);
            cpu_stats.push_back(idle);
            cpu_stats.push_back(iowait);
            cpu_stats.push_back(irq);
            cpu_stats.push_back(softirq);
            cpu_stats.push_back(steal);
        }

        return cpu_stats;
    }

    double calculate_cpu_usage(const std::vector<unsigned long long>& prev_stats, const std::vector<unsigned long long>& curr_stats)
    {
        unsigned long long prev_idle = prev_stats[3] + prev_stats[4];
        unsigned long long curr_idle = curr_stats[3] + curr_stats[4];

        unsigned long long prev_non_idle = prev_stats[0] + prev_stats[1] + prev_stats[2] + prev_stats[5] + prev_stats[6] + prev_stats[7];
        unsigned long long curr_non_idle = curr_stats[0] + curr_stats[1] + curr_stats[2] + curr_stats[5] + curr_stats[6] + curr_stats[7];

        unsigned long long prev_total = prev_idle + prev_non_idle;
        unsigned long long curr_total = curr_idle + curr_non_idle;

        unsigned long long total_diff = curr_total - prev_total;
        unsigned long long idle_diff = curr_idle - prev_idle;

        // Calculate the CPU usage as a percentage
        return (1.0 - static_cast<double>(idle_diff) / total_diff) * 100;
    }

    void cpu_usage_monitor()
    {
        std::vector<unsigned long long> prev_stats = get_cpu_stats();

        while (m_running) {
            // Wait for a second before getting stats again
            std::this_thread::sleep_for(std::chrono::seconds(12));

            // Get current CPU stats
            std::vector<unsigned long long> curr_stats = get_cpu_stats();

            // Calculate and print CPU usage percentage
            double cpu_usage = calculate_cpu_usage(prev_stats, curr_stats);
            if (cpu_usage > m_max_cpu) { m_max_cpu = cpu_usage; }
            std::cout << "CPU Usage: " << cpu_usage << "%" << std::endl;

            // Update previous stats
            prev_stats = curr_stats;
        }

        std::cout << "Max CPU Usage: " << m_max_cpu << "%" << std::endl;
    }
};
#endif

#include <chrono>

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

#include <x86intrin.h> // For __rdtsc()

class MeasureTime {

  public:
    MeasureTime(unsigned long long& max) : m_max(max)
    {
        m_start = __rdtsc();
    }

    ~MeasureTime()
    {
        unsigned long long end = __rdtsc();
        auto e = end - m_start;
        std::cout << "CPU Cycles: " << (end - m_start) << "\t\t" << m_max << "\n";
        if (e > m_max) m_max = e;
    }

  private:
    unsigned long long& m_max;
    unsigned long long m_start;
};

class CpuTimer {

  public:
    CpuTimer() {}

    void start() {
        m_start = __rdtsc();
    }

    void print() {
        std::cout << "CPU Cycles: " << (m_end - m_start) << "\t\t" << m_max << "\n";
    }

    void stop()
    {
        m_end = __rdtsc();
        auto e = m_end - m_start;
        if (e > m_max) m_max = e;

        m_index++;
        if (m_index % 200 == 0) {
            print();
        }
    }


  private:
    unsigned long long m_index;
    unsigned long long m_max;
    unsigned long long m_start;
    unsigned long long m_end;
};
