// nanobind bindings for cpy6107::framework::TimedCommandRobot.
//
// Python names follow robotpy conventions (camelCase, seconds as float) so that a robot class can
// be a drop-in for commands2.TimedCommandRobot. The class is duck-typed against
// wpilib.run(): it only needs startCompetition() / endCompetition().

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

using cpy6107::framework::LoopTiming;
using cpy6107::framework::TimedCommandRobot;
using wpi::units::second_t;

[[nodiscard]] double ToSeconds(std::chrono::nanoseconds duration) {
    return std::chrono::duration<double>(duration).count();
}

// Each virtual re-acquires the GIL: the loop runs with the GIL released (see the startCompetition
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
    CPY_VIRTUAL(driverStationConnected, DriverStationConnected)
    CPY_VIRTUAL(simulationInit, SimulationInit)
    CPY_VIRTUAL(disabledInit, DisabledInit)
    CPY_VIRTUAL(autonomousInit, AutonomousInit)
    CPY_VIRTUAL(teleopInit, TeleopInit)
    CPY_VIRTUAL(utilityInit, UtilityInit)
    CPY_VIRTUAL(robotPeriodic, RobotPeriodic)
    CPY_VIRTUAL(simulationPeriodic, SimulationPeriodic)
    CPY_VIRTUAL(disabledPeriodic, DisabledPeriodic)
    CPY_VIRTUAL(autonomousPeriodic, AutonomousPeriodic)
    CPY_VIRTUAL(teleopPeriodic, TeleopPeriodic)
    CPY_VIRTUAL(utilityPeriodic, UtilityPeriodic)
    CPY_VIRTUAL(disabledExit, DisabledExit)
    CPY_VIRTUAL(autonomousExit, AutonomousExit)
    CPY_VIRTUAL(teleopExit, TeleopExit)
    CPY_VIRTUAL(utilityExit, UtilityExit)

    void BeforeUserLoop() override {
        nanobind::gil_scoped_acquire gil;
        NB_OVERRIDE_NAME("beforeUserLoop", BeforeUserLoop);
    }

    void AfterUserLoop(const LoopTiming& timing) override {
        nanobind::gil_scoped_acquire gil;
        NB_OVERRIDE_NAME("afterUserLoop", AfterUserLoop, timing);
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
// without re-entering the trampoline, so `super().teleopPeriodic()` cannot recurse.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CPY_DEFAULT(cls, py_name, cpp_name, doc)                                                   \
    cls.def(py_name, [](TimedCommandRobot& self) { self.wpi::IterativeRobotBase::cpp_name(); }, doc)

} // namespace

// NB_MODULE expands to a non-const global, a C-style cast and a vararg call.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-pro-type-cstyle-cast,cppcoreguidelines-pro-type-vararg,performance-unnecessary-value-param)
NB_MODULE(_cpy6107, m) {
    namespace nb = nanobind;

    m.doc() = "C++23 TimedCommandRobot with a settable loop watchdog for FRC 2027 (robotpy).";

    nb::class_<LoopTiming>(m, "LoopTiming",
                           "Time spent in the two phases of one main-loop cycle (seconds).")
        .def_prop_ro(
            "pre_user_time", [](const LoopTiming& t) { return ToSeconds(t.pre_user_time); },
            "Seconds spent in beforeUserLoop().")
        .def_prop_ro(
            "user_time", [](const LoopTiming& t) { return ToSeconds(t.user_time); },
            "Seconds spent in the mode periodic, robotPeriodic and simulationPeriodic methods.");

    auto robot = nb::class_<TimedCommandRobot, PyTimedCommandRobot>(
        m, "TimedCommandRobot",
        "Periodic robot base class that owns its loop watchdog so the timeout can be set.\n\n"
        "Subclass it and override the usual robotpy hooks (robotPeriodic, teleopPeriodic, ...). "
        "Pass the class to wpilib.run(). Each period one slot runs the robot loop; a second slot, "
        "offset by `scheduler_offset`, runs the callable given to setScheduler().",
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
        .def("startCompetition", &TimedCommandRobot::StartCompetition,
             nb::call_guard<nb::gil_scoped_release>(),
             "Run the robot loop until endCompetition() is called. Releases the GIL while waiting.")
        .def("endCompetition", &TimedCommandRobot::EndCompetition,
             nb::call_guard<nb::gil_scoped_release>(),
             "Stop the loop; safe to call from any thread.")
        .def(
            "addPeriodic",
            [](TimedCommandRobot& self, std::function<void()> callback, double period,
               double offset) {
                self.AddPeriodic(std::move(callback), second_t{period}, second_t{offset});
            },
            nb::arg("callback"), nb::arg("period"), nb::arg("offset") = 0.0,
            "Call `callback` every `period` seconds, `offset` seconds after the common start "
            "time, on the robot thread. Call before startCompetition().")
        .def(
            "setScheduler",
            [](TimedCommandRobot& self, std::function<void()> scheduler) {
                self.SetScheduler(std::move(scheduler));
            },
            nb::arg("scheduler"),
            "Set the command scheduler callable, run once per period at the scheduler offset. "
            "Call before startCompetition().")
        .def("clearScheduler", &TimedCommandRobot::ClearScheduler, "Remove the scheduler callable.")
        .def(
            "setWatchdogTimeout",
            [](TimedCommandRobot& self, double timeout) {
                if (!self.SetWatchdogTimeout(second_t{timeout})) {
                    throw nb::value_error("watchdog timeout must be positive");
                }
            },
            nb::arg("timeout"),
            "Set the loop overrun threshold in seconds (default: the loop period). Raises "
            "ValueError if timeout <= 0.")
        .def(
            "getWatchdogTimeout",
            [](const TimedCommandRobot& self) { return self.GetWatchdogTimeout().value(); },
            "Loop overrun threshold in seconds.")
        .def(
            "getPeriod", [](const TimedCommandRobot& self) { return self.GetPeriod().value(); },
            "Loop period in seconds.")
        .def(
            "addWatchdogEpoch",
            [](TimedCommandRobot& self, const std::string& name) { self.AddWatchdogEpoch(name); },
            nb::arg("name"), "Add a named epoch to the current cycle's watchdog.")
        .def("printWatchdogEpochs", &TimedCommandRobot::PrintLoopWatchdogEpochs,
             "Print the current cycle's watchdog epochs.")
        .def(
            "getLoopStartTime",
            [](const TimedCommandRobot& self) { return ToSeconds(self.GetLoopStartTime()); },
            "Monotonic time, in seconds, at the start of the current periodic callback.")
        .def(
            "beforeUserLoop", [](TimedCommandRobot& self) { AsPy(self).DefaultBeforeUserLoop(); },
            "Override: runs just before the loop body, outside the watchdog window.")
        .def(
            "afterUserLoop",
            [](TimedCommandRobot& self, const LoopTiming& timing) {
                AsPy(self).DefaultAfterUserLoop(timing);
            },
            nb::arg("timing"),
            "Override: runs just after the loop body, outside the watchdog window.");

    CPY_DEFAULT(robot, "driverStationConnected", DriverStationConnected,
                "Override: called once when the driver station first connects.");
    CPY_DEFAULT(robot, "simulationInit", SimulationInit, "Override: simulation initialization.");
    CPY_DEFAULT(robot, "disabledInit", DisabledInit, "Override: entering disabled.");
    CPY_DEFAULT(robot, "autonomousInit", AutonomousInit, "Override: entering autonomous.");
    CPY_DEFAULT(robot, "teleopInit", TeleopInit, "Override: entering teleoperated.");
    CPY_DEFAULT(robot, "utilityInit", UtilityInit, "Override: entering utility mode.");
    CPY_DEFAULT(robot, "robotPeriodic", RobotPeriodic, "Override: runs every cycle in all modes.");
    CPY_DEFAULT(robot, "simulationPeriodic", SimulationPeriodic,
                "Override: runs every cycle in simulation.");
    CPY_DEFAULT(robot, "disabledPeriodic", DisabledPeriodic, "Override: periodic while disabled.");
    CPY_DEFAULT(robot, "autonomousPeriodic", AutonomousPeriodic,
                "Override: periodic during autonomous.");
    CPY_DEFAULT(robot, "teleopPeriodic", TeleopPeriodic, "Override: periodic during teleop.");
    CPY_DEFAULT(robot, "utilityPeriodic", UtilityPeriodic,
                "Override: periodic during utility mode.");
    CPY_DEFAULT(robot, "disabledExit", DisabledExit, "Override: leaving disabled.");
    CPY_DEFAULT(robot, "autonomousExit", AutonomousExit, "Override: leaving autonomous.");
    CPY_DEFAULT(robot, "teleopExit", TeleopExit, "Override: leaving teleoperated.");
    CPY_DEFAULT(robot, "utilityExit", UtilityExit, "Override: leaving utility mode.");
}
