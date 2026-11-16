// nanobind bindings for cpy6107::framework::TimedCommandRobot.
//
// Python names follow robotpy 2027 conventions (snake_case, seconds as float) so that a robot class
// can be a drop-in for commands2.TimedCommandRobot. The class is duck-typed against wpilib.run():
// it only needs start_competition() / end_competition().

#include "cpy6107/framework/timed_command_robot.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/string.h>
#include <nanobind/trampoline.h>

#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using cpy6107::framework::HookErrorPolicy;
using cpy6107::framework::LoopTiming;
using cpy6107::framework::OverrunInfo;
using cpy6107::framework::TimedCommandRobot;
using wpi::units::second_t;

[[nodiscard]] double ToSeconds(std::chrono::nanoseconds duration) {
    return std::chrono::duration<double>(duration).count();
}

// Each virtual re-acquires the GIL: the loop runs with the GIL released (see the start_competition
// binding) so other Python threads keep running between cycles.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CPY_VIRTUAL(py_name, cpp_name)                                                             \
    void cpp_name() override {                                                                     \
        nanobind::gil_scoped_acquire gil;                                                          \
        NB_OVERRIDE_NAME(#py_name, cpp_name);                                                      \
    }

class PyTimedCommandRobot : public TimedCommandRobot {
  public:
    NB_TRAMPOLINE(TimedCommandRobot, 18);

    PyTimedCommandRobot(double period, double scheduler_offset)
        : TimedCommandRobot(second_t{period}, second_t{scheduler_offset}) {}

    // NOLINTBEGIN(readability-else-after-return) -- from NB_OVERRIDE_NAME
    CPY_VIRTUAL(driver_station_connected, DriverStationConnected)
    CPY_VIRTUAL(simulation_init, SimulationInit)
    CPY_VIRTUAL(disabled_init, DisabledInit)
    CPY_VIRTUAL(autonomous_init, AutonomousInit)
    CPY_VIRTUAL(teleop_init, TeleopInit)
    CPY_VIRTUAL(utility_init, UtilityInit)
    CPY_VIRTUAL(robot_periodic, RobotPeriodic)
    CPY_VIRTUAL(simulation_periodic, SimulationPeriodic)
    CPY_VIRTUAL(disabled_periodic, DisabledPeriodic)
    CPY_VIRTUAL(autonomous_periodic, AutonomousPeriodic)
    CPY_VIRTUAL(teleop_periodic, TeleopPeriodic)
    CPY_VIRTUAL(utility_periodic, UtilityPeriodic)
    CPY_VIRTUAL(disabled_exit, DisabledExit)
    CPY_VIRTUAL(autonomous_exit, AutonomousExit)
    CPY_VIRTUAL(teleop_exit, TeleopExit)
    CPY_VIRTUAL(utility_exit, UtilityExit)

    void BeforeUserLoop() override {
        nanobind::gil_scoped_acquire gil;
        NB_OVERRIDE_NAME("before_user_loop", BeforeUserLoop);
    }

    void AfterUserLoop(const LoopTiming& timing) override {
        nanobind::gil_scoped_acquire gil;
        NB_OVERRIDE_NAME("after_user_loop", AfterUserLoop, timing);
    }
    // NOLINTEND(readability-else-after-return)

    // Non-virtual entry points to the C++ defaults, for the Python base-class methods.
    void DefaultBeforeUserLoop() { TimedCommandRobot::BeforeUserLoop(); }
    void DefaultAfterUserLoop(const LoopTiming& timing) {
        TimedCommandRobot::AfterUserLoop(timing);
    }
};

PyTimedCommandRobot& AsPy(TimedCommandRobot& robot) {
    // Every instance created from Python is a PyTimedCommandRobot.
    auto* py_robot = dynamic_cast<PyTimedCommandRobot*>(&robot);
    if (py_robot == nullptr) {
        throw std::logic_error("TimedCommandRobot instance was not created from Python");
    }
    return *py_robot;
}

// Python-visible default for a public IterativeRobotBase virtual: runs the C++ implementation
// without re-entering the trampoline, so `super().teleop_periodic()` cannot recurse.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CPY_DEFAULT(cls, py_name, cpp_name, doc)                                                   \
    cls.def(py_name, [](TimedCommandRobot& self) { self.wpi::IterativeRobotBase::cpp_name(); }, doc)

} // namespace

// NB_MODULE expands to a non-const global, a C-style cast and a vararg call.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-pro-type-cstyle-cast,cppcoreguidelines-pro-type-vararg,performance-unnecessary-value-param)
NB_MODULE(_cpy6107, m) {
    namespace nb = nanobind;

    m.doc() = "C++23 TimedCommandRobot with a settable loop watchdog for FRC 2027 (robotpy).";

    nb::enum_<HookErrorPolicy>(m, "HookErrorPolicy",
                               "What to do when a before/after_user_loop hook or the overrun "
                               "callback raises.")
        .value("LOG_AND_CONTINUE", HookErrorPolicy::kLogAndContinue,
               "Report the error and keep the loop running (default).")
        .value("STOP", HookErrorPolicy::kStop, "Re-raise out of start_competition().");

    nb::class_<OverrunInfo>(m, "OverrunInfo", "One loop cycle that exceeded the watchdog timeout.")
        .def_prop_ro(
            "loop_time", [](const OverrunInfo& o) { return ToSeconds(o.loop_time); },
            "Seconds spent in before_user_loop() plus the loop body.")
        .def_prop_ro(
            "timeout", [](const OverrunInfo& o) { return o.timeout.value(); },
            "Watchdog timeout in seconds.");

    nb::class_<LoopTiming>(m, "LoopTiming",
                           "Time spent in the two phases of one main-loop cycle (seconds).")
        .def_prop_ro(
            "pre_user_time", [](const LoopTiming& t) { return ToSeconds(t.pre_user_time); },
            "Seconds spent in before_user_loop().")
        .def_prop_ro(
            "user_time", [](const LoopTiming& t) { return ToSeconds(t.user_time); },
            "Seconds spent in the mode periodic, robot_periodic and simulation_periodic methods.");

    auto robot = nb::class_<TimedCommandRobot, PyTimedCommandRobot>(
        m, "TimedCommandRobot",
        "Periodic robot base class that owns its loop watchdog so the timeout can be set.\n\n"
        "Subclass it and override the usual robotpy hooks (robot_periodic, teleop_periodic, ...). "
        "Pass the class to wpilib.run(). Each period one slot runs the robot loop; a second slot, "
        "offset by `scheduler_offset`, runs the callable given to set_scheduler().",
        nb::dynamic_attr());

    robot
        .def(nb::init<double, double>(),
             nb::arg("period") = TimedCommandRobot::kDefaultPeriod.value(),
             nb::arg("scheduler_offset") = TimedCommandRobot::kSchedulerOffset.value(),
             "Create the robot. `period` and `scheduler_offset` are in seconds; raises ValueError "
             "if period <= 0 or scheduler_offset < 0.")
        .def_prop_ro_static(
            "DEFAULT_PERIOD", [](nb::handle) { return TimedCommandRobot::kDefaultPeriod.value(); },
            "Default loop period in seconds (0.02).")
        .def_prop_ro_static(
            "SCHEDULER_OFFSET",
            [](nb::handle) { return TimedCommandRobot::kSchedulerOffset.value(); },
            "Default offset of the scheduler slot from the loop slot, in seconds (0.005).")
        .def(
            "start_competition", &TimedCommandRobot::StartCompetition,
            nb::call_guard<nb::gil_scoped_release>(),
            "Run the robot loop until end_competition() is called. Releases the GIL while waiting.")
        .def("end_competition", &TimedCommandRobot::EndCompetition,
             nb::call_guard<nb::gil_scoped_release>(),
             "Stop the loop; safe to call from any thread.")
        .def(
            "add_periodic",
            [](TimedCommandRobot& self, std::function<void()> callback, double period,
               double offset) {
                self.AddPeriodic(std::move(callback), second_t{period}, second_t{offset});
            },
            nb::arg("callback"), nb::arg("period"), nb::arg("offset") = 0.0,
            "Call `callback` every `period` seconds, `offset` seconds after the common start "
            "time, on the robot thread. Call before start_competition().")
        .def(
            "set_scheduler",
            [](TimedCommandRobot& self, std::function<void()> scheduler) {
                self.SetScheduler(std::move(scheduler));
            },
            nb::arg("scheduler"),
            "Set the command scheduler callable, run once per period at the scheduler offset. "
            "Call before start_competition().")
        .def("clear_scheduler", &TimedCommandRobot::ClearScheduler,
             "Remove the scheduler callable.")
        .def(
            "set_watchdog_timeout",
            [](TimedCommandRobot& self, double timeout) {
                if (!self.SetWatchdogTimeout(second_t{timeout})) {
                    throw nb::value_error("watchdog timeout must be positive");
                }
            },
            nb::arg("timeout"),
            "Set the loop overrun threshold in seconds (default: the loop period). Raises "
            "ValueError if timeout <= 0.")
        .def(
            "get_watchdog_timeout",
            [](const TimedCommandRobot& self) { return self.GetWatchdogTimeout().value(); },
            "Loop overrun threshold in seconds.")
        .def(
            "get_period", [](const TimedCommandRobot& self) { return self.GetPeriod().value(); },
            "Loop period in seconds.")
        .def(
            "add_watchdog_epoch",
            [](TimedCommandRobot& self, const std::string& name) { self.AddWatchdogEpoch(name); },
            nb::arg("name"), "Add a named epoch to the current cycle's watchdog.")
        .def("print_watchdog_epochs", &TimedCommandRobot::PrintLoopWatchdogEpochs,
             "Print the current cycle's watchdog epochs.")
        .def(
            "get_loop_start_time",
            [](const TimedCommandRobot& self) { return ToSeconds(self.GetLoopStartTime()); },
            "Monotonic time, in seconds, at the start of the current periodic callback.")
        .def(
            "set_hook_error_policy",
            [](TimedCommandRobot& self, HookErrorPolicy policy) {
                self.SetHookErrorPolicy(policy);
            },
            nb::arg("policy"),
            "Choose what happens when before_user_loop, after_user_loop or the overrun callback "
            "raises. Default LOG_AND_CONTINUE. Call before start_competition().")
        .def("get_hook_error_policy", &TimedCommandRobot::GetHookErrorPolicy,
             "Current hook error policy.")
        .def("get_hook_error_count", &TimedCommandRobot::GetHookErrorCount,
             "Number of hook exceptions swallowed under LOG_AND_CONTINUE.")
        .def(
            "set_overrun_callback",
            [](TimedCommandRobot& self, std::function<void(const OverrunInfo&)> callback) {
                self.SetOverrunCallback(std::move(callback));
            },
            nb::arg("callback").none(),
            "Call `callback(info: OverrunInfo)` on the robot thread after each cycle whose "
            "watchdog expired. Pass None to clear. Call before start_competition().")
        .def(
            "before_user_loop", [](TimedCommandRobot& self) { AsPy(self).DefaultBeforeUserLoop(); },
            "Override: runs just before the loop body, outside the watchdog window.")
        .def(
            "after_user_loop",
            [](TimedCommandRobot& self, const LoopTiming& timing) {
                AsPy(self).DefaultAfterUserLoop(timing);
            },
            nb::arg("timing"),
            "Override: runs just after the loop body, outside the watchdog window.")
        // The driver station / runtime queries of wpilib.RobotBase, which this class does not
        // derive from in Python.
        .def_static(
            "is_real", [] { return wpi::RobotBase::IsReal(); }, "True when running on a robot.")
        .def_static(
            "is_simulation", [] { return wpi::RobotBase::IsSimulation(); },
            "True when running in simulation.")
        .def_static(
            "is_enabled", [] { return wpi::RobotBase::IsEnabled(); },
            "True when the robot is enabled.")
        .def_static(
            "is_disabled", [] { return wpi::RobotBase::IsDisabled(); },
            "True when the robot is disabled.")
        .def_static(
            "is_autonomous", [] { return wpi::RobotBase::IsAutonomous(); },
            "True in autonomous mode, enabled or not.")
        .def_static(
            "is_autonomous_enabled", [] { return wpi::RobotBase::IsAutonomousEnabled(); },
            "True when enabled in autonomous mode.")
        .def_static(
            "is_teleop", [] { return wpi::RobotBase::IsTeleop(); },
            "True in teleoperated mode, enabled or not.")
        .def_static(
            "is_teleop_enabled", [] { return wpi::RobotBase::IsTeleopEnabled(); },
            "True when enabled in teleoperated mode.")
        .def_static(
            "is_utility", [] { return wpi::RobotBase::IsUtility(); },
            "True in utility mode, enabled or not.")
        .def_static(
            "is_utility_enabled", [] { return wpi::RobotBase::IsUtilityEnabled(); },
            "True when enabled in utility mode.")
        .def_static(
            "get_opmode_id", [] { return wpi::RobotBase::GetOpModeId(); },
            "Id of the selected operation mode.")
        .def_static(
            "get_opmode", [] { return wpi::RobotBase::GetOpMode(); },
            "Name of the selected operation mode.");

    CPY_DEFAULT(robot, "driver_station_connected", DriverStationConnected,
                "Override: called once when the driver station first connects.");
    CPY_DEFAULT(robot, "simulation_init", SimulationInit, "Override: simulation initialization.");
    CPY_DEFAULT(robot, "disabled_init", DisabledInit, "Override: entering disabled.");
    CPY_DEFAULT(robot, "autonomous_init", AutonomousInit, "Override: entering autonomous.");
    CPY_DEFAULT(robot, "teleop_init", TeleopInit, "Override: entering teleoperated.");
    CPY_DEFAULT(robot, "utility_init", UtilityInit, "Override: entering utility mode.");
    CPY_DEFAULT(robot, "robot_periodic", RobotPeriodic, "Override: runs every cycle in all modes.");
    CPY_DEFAULT(robot, "simulation_periodic", SimulationPeriodic,
                "Override: runs every cycle in simulation.");
    CPY_DEFAULT(robot, "disabled_periodic", DisabledPeriodic, "Override: periodic while disabled.");
    CPY_DEFAULT(robot, "autonomous_periodic", AutonomousPeriodic,
                "Override: periodic during autonomous.");
    CPY_DEFAULT(robot, "teleop_periodic", TeleopPeriodic, "Override: periodic during teleop.");
    CPY_DEFAULT(robot, "utility_periodic", UtilityPeriodic,
                "Override: periodic during utility mode.");
    CPY_DEFAULT(robot, "disabled_exit", DisabledExit, "Override: leaving disabled.");
    CPY_DEFAULT(robot, "autonomous_exit", AutonomousExit, "Override: leaving autonomous.");
    CPY_DEFAULT(robot, "teleop_exit", TeleopExit, "Override: leaving teleoperated.");
    CPY_DEFAULT(robot, "utility_exit", UtilityExit, "Override: leaving utility mode.");
}
