// ------------------------------------------------------------------------
//      o-o      o                o
//     /         |                |
//    O     o  o O-o  o-o o-o     |  oo o--o o-o o-o
//     \    |  | |  | |-' |   \   o | | |  |  /   /
//      o-o o--O o-o  o-o o    o-o  o-o-o--O o-o o-o
//             |                           |
//          o--o                        o--o
//                        o--o      o         o
//                        |   |     |         |  o
//                        O-Oo  o-o O-o  o-o -o-    o-o o-o
//                        |  \  | | |  | | |  |  | |     |
//                        o   o o-o o-o  o-o  o  |  o-o o-o
//
//    Jemison High School - Huntsville Alabama
// ------------------------------------------------------------------------

#pragma once

// WPILib release adapter.
//
// The framework layer re-implements parts of wpi::TimedRobot / wpi::IterativeRobotBase, whose
// internals changed between 2027 alpha releases. Everything that differs between releases is
// isolated here and in the "release-specific" section of src/framework/timed_command_robot.cpp.
//
// Supported: robotpy / WPILib 2027.0.0-alpha-6 and alpha-8 (a7 is expected to behave like a8 but
// is not tested). The releases differ in:
//   alpha-6: monotonic time and PeriodicPriorityQueue in microseconds; LoopFunc() calls
//            SmartDashboard::UpdateValues().
//   alpha-8: nanoseconds; LoopFunc() calls wpi::tunables::TunableRegistry::Update();
//            SmartDashboard.hpp no longer exists; DriverStationTypes.h became .hpp.
// The release is detected from the presence of <wpi/smartdashboard/SmartDashboard.hpp>.

#include <wpi/system/RobotController.hpp>

#include <chrono>

#if __has_include(<wpi/smartdashboard/SmartDashboard.hpp>)
#define CPY6107_WPILIB_ALPHA6 1
#else
#define CPY6107_WPILIB_ALPHA6 0
#endif

namespace cpy6107::framework::compat {

// Resolution of the HAL monotonic clock and of PeriodicPriorityQueue start times.
#if CPY6107_WPILIB_ALPHA6
using MonotonicDuration = std::chrono::microseconds;
#else
using MonotonicDuration = std::chrono::nanoseconds;
#endif

[[nodiscard]] inline std::chrono::nanoseconds MonotonicNow() noexcept {
    return MonotonicDuration{
        static_cast<MonotonicDuration::rep>(wpi::RobotController::GetMonotonicTime())};
}

} // namespace cpy6107::framework::compat
