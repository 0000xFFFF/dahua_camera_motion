
#include "signal.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <ostream>

#include "motion_detector.hpp"

extern std::unique_ptr<MotionDetector> motionDetector;

void init_signal()
{
    // Add signal handling
    std::signal(SIGINT, [](int) {
        motionDetector->stop();
        motionDetector.release();
        std::cout << "SIGINT" << std::endl;
        std::exit(0);
    });
}
