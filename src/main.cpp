#include <iostream>
#include <opencv2/opencv.hpp>
#include <sched.h>

#include "args.hpp"
#include "debug.hpp"
#include "motion_detector.hpp"

#include "signal.hpp"
#include "sound.hpp"

#ifdef DEBUG_CPU
#include "debug.hpp"
#endif

std::unique_ptr<MotionDetector> motionDetector;

int main(int argc, char* argv[])
{
    // Add near the start of main():
    std::cout << "OpenCV build info:\n"
              << cv::getBuildInformation() << std::endl;
    std::cout << "OpenCL available: " << cv::ocl::haveOpenCL() << std::endl;
    if (cv::ocl::haveOpenCL()) {
        cv::ocl::setUseOpenCL(true);
        std::vector<cv::ocl::PlatformInfo> platforms;
        cv::ocl::getPlatfomsInfo(platforms);
        for (const auto& p : platforms) {
            std::cout << "Platform: " << p.name() << std::endl;
        }
    }

    init_signal();

#ifdef DEBUG_CPU
    CpuUsageMonitor monitor;
#endif

    auto program = parse_args();
    program->parse_args(argc, argv);

    init_sound();

    MotionDetectorParams params(program);

    {
        motionDetector = std::make_unique<MotionDetector>(params);
        motionDetector->draw_loop();
    }

    uninit_sound();

    DPL("main return 0");
    return 0;
}
