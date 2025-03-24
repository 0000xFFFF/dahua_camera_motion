#include <argparse/argparse.hpp>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <sched.h>
#include <string>

#include "globals.hpp"
#include "motion_detector.hpp"
#include "utils.h"

#ifdef DEBUG

class CpuUsageMonitor {


  public:
    CpuUsageMonitor()
    {
        m_thread = std::thread([this]() { cpu_usage_monitor(); });
    }

    ~CpuUsageMonitor() {
        m_running = false;
        m_thread.join();
    }


  private:
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
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Get current CPU stats
            std::vector<unsigned long long> curr_stats = get_cpu_stats();

            // Calculate and print CPU usage percentage
            double cpu_usage = calculate_cpu_usage(prev_stats, curr_stats);
            std::cout << "CPU Usage: " << cpu_usage << "%" << std::endl;

            // Update previous stats
            prev_stats = curr_stats;
        }
    }
};
#endif

int main(int argc, char* argv[])
{

    // Add signal handling
    std::signal(SIGINT, [](int) {
        cv::destroyAllWindows();
        std::exit(0);
    });


#ifdef DEBUG
    CpuUsageMonitor monitor;
#endif

    argparse::ArgumentParser program("dcm_master");

    program.add_argument("-i", "--ip")
        .help("IP to connect to")
        .required();
    program.add_argument("-u", "--username")
        .help("Account username")
        .required();
    program.add_argument("-p", "--password")
        .help("Account password")
        .required();
    program.add_argument("-a", "--area")
        .help("Contour area for detection")
        .default_value(MOTION_DETECT_AREA)
        .scan<'i', int>();
    program.add_argument("-ww", "--width")
        .help("Window width")
        .default_value(1920)
        .scan<'i', int>();
    program.add_argument("-wh", "--height")
        .help("Window height")
        .default_value(1080)
        .scan<'i', int>();
    program.add_argument("-fs", "--fullscreen")
        .help("Start in fullscreen mode")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-d", "--detect")
        .help("Detect screen size with xrandr")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-r", "--resolution")
        .help("index of resolution to use (default: 0)")
        .default_value(0)
        .scan<'i', int>();

    try {
        program.parse_args(argc, argv);

        int width = program.get<int>("width");
        int height = program.get<int>("height");

        // Override width/height with detected screen size if requested
        if (program.get<bool>("detect")) {
            auto [detected_width, detected_height] = detect_screen_size(program.get<int>("resolution"));
            width = detected_width;
            height = detected_height;
            std::cout << "Detected screen size: " << width << "x" << height << std::endl;
        }

        {
            MotionDetector motionDetector(
                program.get<std::string>("ip"),
                program.get<std::string>("username"),
                program.get<std::string>("password"),
                program.get<int>("area"), width, height, program.get<bool>("fullscreen"));

            motionDetector.start();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
