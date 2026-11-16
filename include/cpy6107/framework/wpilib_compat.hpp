#pragma once

// WPILib release adapter.
//
// The framework layer re-implements parts of wpi::TimedRobot / wpi::IterativeRobotBase, whose
// internals changed between 2027 alpha releases. Everything that differs between releases is
// isolated here and in the "release-specific" section of src/framework/timed_command_robot.cpp.
//
// Supported: robotpy / WPILib 2027.0.0-alpha-6 (monotonic time in microseconds, SmartDashboard
// update in LoopFunc). Later alphas (a7+) switched to nanosecond monotonic time, wpi::util::Alert
// and wpi::util::Handle and are not yet supported.

#include <wpi/system/RobotController.hpp>

#include <chrono>

#if __has_include(<wpi/util/Alert.hpp>)
#error "cpy6107::framework supports WPILib 2027 alpha-6 only; see docs/PLAN.md (a8 follow-up)."
#endif

namespace cpy6107::framework::compat {

// Resolution of the HAL monotonic clock and of PeriodicPriorityQueue start times.
using MonotonicDuration = std::chrono::microseconds;

[[nodiscard]] inline std::chrono::nanoseconds MonotonicNow() noexcept {
    return MonotonicDuration{
        static_cast<MonotonicDuration::rep>(wpi::RobotController::GetMonotonicTime())};
}

} // namespace cpy6107::framework::compat
