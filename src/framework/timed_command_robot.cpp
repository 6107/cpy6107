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
//                        |  \  | | |  | | |  |  | |     \
//                        o   o o-o o-o  o-o  o  |  o-o o-o
//
//    Jemison High School - Huntsville Alabama
// ------------------------------------------------------------------------

#include "cpy6107/framework/timed_command_robot.hpp"

#include <wpi/driverstation/RobotState.hpp>
#include <wpi/driverstation/internal/DriverStationBackend.hpp>
#include <wpi/hal/DriverStation.h>
#include <wpi/hal/DriverStation.hpp>
#if CPY6107_WPILIB_ALPHA6
#include <wpi/hal/DriverStationTypes.h>
#include <wpi/smartdashboard/SmartDashboard.hpp>
#else
#include <wpi/hal/DriverStationTypes.hpp>
#include <wpi/tunables/TunableRegistry.hpp>
#endif
#include <wpi/hal/Notifier.hpp>
#include <wpi/hal/UsageReporting.hpp>
#include <wpi/nt/NetworkTableInstance.hpp>
#include <wpi/system/Errors.hpp>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>

namespace cpy6107::framework {

// File-local shorthands; never put these in a header.
namespace chrono = std::chrono;
using wpi::RobotMode;
using wpi::units::hertz_t;
using wpi::units::second_t;

namespace {

// ---- release-specific: see wpilib_compat.hpp --------------------------------------------------

void UpdateTelemetry(wpi::Watchdog& watchdog) {
#if CPY6107_WPILIB_ALPHA6
    wpi::SmartDashboard::UpdateValues();
    watchdog.AddEpoch("SmartDashboard::UpdateValues()");
#else
    wpi::tunables::TunableRegistry::Update();
    watchdog.AddEpoch("TunableRegistry::Update()");
#endif
}

// -----------------------------------------------------------------------------------------------

[[nodiscard]] chrono::nanoseconds ToNanoseconds(second_t seconds) {
    return chrono::duration_cast<chrono::nanoseconds>(chrono::duration<double>{seconds.value()});
}

} // namespace

TimedCommandRobot::TimedCommandRobot(second_t period, second_t scheduler_offset)
    : wpi::IterativeRobotBase(period), watchdog_(period, [this] { ReportLoopOverrun(); }) {
    if (!(period.value() > 0.0)) {
        throw std::invalid_argument("TimedCommandRobot: period must be positive");
    }
    if (!(scheduler_offset.value() >= 0.0)) {
        throw std::invalid_argument("TimedCommandRobot: scheduler_offset must not be negative");
    }

    start_time_ = chrono::duration_cast<compat::MonotonicDuration>(compat::MonotonicNow());
    callbacks_.Add([this] { ExecuteLoopCycle(); }, start_time_, period);
    callbacks_.Add(
        [this] {
            if (scheduler_) {
                scheduler_();
            }
        },
        start_time_, period, scheduler_offset);

    int32_t status = 0;
    const HAL_NotifierHandle handle = HAL_CreateNotifier(&status);
    WPILIB_CheckErrorStatus(status, "InitializeNotifier");
    notifier_.store(handle);
    HAL_SetNotifierName(handle, "TimedCommandRobot", &status);

    HAL_ReportUsage("Framework", "TimedCommandRobot");
}

TimedCommandRobot::TimedCommandRobot(hertz_t frequency) : TimedCommandRobot{1 / frequency} {}

TimedCommandRobot::~TimedCommandRobot() {
    const HAL_NotifierHandle handle = notifier_.exchange(HAL_INVALID_HANDLE);
    if (handle != HAL_INVALID_HANDLE) {
        HAL_DestroyNotifier(handle);
    }
}

void TimedCommandRobot::StartCompetition() {
    if constexpr (IsSimulation()) {
        SimulationInit();
    }

    std::puts("\n********** Robot program startup complete **********");
    wpi::internal::DriverStationBackend::ObserveUserProgramStarting();

    while (true) {
        const HAL_NotifierHandle handle = notifier_.load();
        if (handle == HAL_INVALID_HANDLE || !callbacks_.RunCallbacks(handle)) {
            break;
        }
    }
}

void TimedCommandRobot::EndCompetition() {
    // Destroying the notifier wakes the loop thread, whose wait then reports "stopped".
    const HAL_NotifierHandle handle = notifier_.exchange(HAL_INVALID_HANDLE);
    if (handle != HAL_INVALID_HANDLE) {
        HAL_DestroyNotifier(handle);
    }
}

void TimedCommandRobot::AddPeriodic(std::function<void()> callback, second_t period,
                                    second_t offset) {
    callbacks_.Add(std::move(callback), start_time_, period, offset);
}

void TimedCommandRobot::SetScheduler(std::function<void()> scheduler) {
    scheduler_ = std::move(scheduler);
}

void TimedCommandRobot::SetScheduler(SchedulerHook& hook) {
    scheduler_ = [&hook] { hook.Run(); };
}

void TimedCommandRobot::ClearScheduler() {
    scheduler_ = nullptr;
}

std::expected<void, FrameworkError> TimedCommandRobot::SetWatchdogTimeout(second_t timeout) {
    if (!(timeout.value() > 0.0)) {
        return std::unexpected(FrameworkError::kInvalidTimeout);
    }
    watchdog_.SetTimeout(timeout);
    return {};
}

second_t TimedCommandRobot::GetWatchdogTimeout() const {
    return watchdog_.GetTimeout();
}

void TimedCommandRobot::AddWatchdogEpoch(std::string_view name) {
    watchdog_.AddEpoch(name);
}

void TimedCommandRobot::PrintLoopWatchdogEpochs() {
    watchdog_.PrintEpochs();
}

chrono::nanoseconds TimedCommandRobot::GetLoopStartTime() const {
    return ToNanoseconds(second_t{callbacks_.GetLoopStartTime()});
}

void TimedCommandRobot::BeforeUserLoop() {}

void TimedCommandRobot::AfterUserLoop(const LoopTiming& /*timing*/) {}

void TimedCommandRobot::SetHookErrorPolicy(HookErrorPolicy policy) {
    hook_error_policy_ = policy;
}

HookErrorPolicy TimedCommandRobot::GetHookErrorPolicy() const {
    return hook_error_policy_;
}

std::uint64_t TimedCommandRobot::GetHookErrorCount() const {
    return hook_error_count_;
}

void TimedCommandRobot::SetOverrunCallback(std::function<void(const OverrunInfo&)> callback) {
    overrun_callback_ = std::move(callback);
}

template <typename F>
void TimedCommandRobot::RunHook(const char* name, const F& hook) {
    try {
        hook();
    } catch (const std::exception& e) {
        if (hook_error_policy_ == HookErrorPolicy::kStop) {
            throw;
        }
        ++hook_error_count_;
        WPILIB_ReportError(wpi::err::Error, "{} threw: {}", name, e.what());
    }
}

void TimedCommandRobot::ExecuteLoopCycle() {
    const auto before_start = compat::MonotonicNow();
    RunHook("before_user_loop", [this] { BeforeUserLoop(); });
    const auto user_start = compat::MonotonicNow();
    LoopBody();
    const auto user_end = compat::MonotonicNow();
    if (overrun_callback_ && watchdog_.IsExpired()) {
        const OverrunInfo info{.loop_time = user_end - before_start,
                               .timeout = watchdog_.GetTimeout()};
        RunHook("overrun callback", [this, &info] { overrun_callback_(info); });
    }
    const LoopTiming timing{.pre_user_time = user_start - before_start,
                            .user_time = user_end - user_start};
    RunHook("after_user_loop", [this, &timing] { AfterUserLoop(timing); });
}

// Mirrors wpi::IterativeRobotBase::LoopFunc() (alpha-6 / alpha-8) with watchdog_ in place of the
// private base watchdog. scripts/check_loopfunc.py diffs it against upstream.
void TimedCommandRobot::LoopBody() {
    wpi::internal::DriverStationBackend::RefreshData();
    watchdog_.Reset();

    // Disabled is treated as the unknown mode.
    const wpi::hal::ControlWord word = wpi::hal::GetControlWord();
    const bool enabled = word.IsEnabled();
    const RobotMode mode = enabled ? word.GetRobotMode() : RobotMode::UNKNOWN;

    if (!called_ds_connected_ && word.IsDSAttached()) {
        called_ds_connected_ = true;
        DriverStationConnected();
    }

    if (last_mode_ != static_cast<int>(mode)) {
        if (last_mode_ == static_cast<int>(RobotMode::UNKNOWN)) {
            DisabledExit();
        } else if (last_mode_ == static_cast<int>(RobotMode::AUTONOMOUS)) {
            AutonomousExit();
        } else if (last_mode_ == static_cast<int>(RobotMode::TELEOPERATED)) {
            TeleopExit();
        } else if (last_mode_ == static_cast<int>(RobotMode::UTILITY)) {
            UtilityExit();
        }

        if (mode == RobotMode::UNKNOWN) {
            DisabledInit();
            watchdog_.AddEpoch("DisabledInit()");
        } else if (mode == RobotMode::AUTONOMOUS) {
            AutonomousInit();
            watchdog_.AddEpoch("AutonomousInit()");
        } else if (mode == RobotMode::TELEOPERATED) {
            TeleopInit();
            watchdog_.AddEpoch("TeleopInit()");
        } else if (mode == RobotMode::UTILITY) {
            UtilityInit();
            watchdog_.AddEpoch("UtilityInit()");
        }

        last_mode_ = static_cast<int>(mode);
    }

    HAL_ObserveUserProgram(word.GetValue());
    if (mode == RobotMode::UNKNOWN) {
        DisabledPeriodic();
        watchdog_.AddEpoch("DisabledPeriodic()");
    } else if (mode == RobotMode::AUTONOMOUS) {
        AutonomousPeriodic();
        watchdog_.AddEpoch("AutonomousPeriodic()");
    } else if (mode == RobotMode::TELEOPERATED) {
        TeleopPeriodic();
        watchdog_.AddEpoch("TeleopPeriodic()");
    } else if (mode == RobotMode::UTILITY) {
        UtilityPeriodic();
        watchdog_.AddEpoch("UtilityPeriodic()");
    }

    RobotPeriodic();
    watchdog_.AddEpoch("RobotPeriodic()");

    UpdateTelemetry(watchdog_);

    if constexpr (IsSimulation()) {
        HAL_SimPeriodicBefore();
        SimulationPeriodic();
        HAL_SimPeriodicAfter();
        watchdog_.AddEpoch("SimulationPeriodic()");
    }

    watchdog_.Disable();

    wpi::nt::NetworkTableInstance::GetDefault().FlushLocal();

    if (watchdog_.IsExpired()) {
        watchdog_.PrintEpochs();
    }
}

void TimedCommandRobot::ReportLoopOverrun() const {
    WPILIB_ReportError(wpi::err::Error, "Loop time of {:.6f}s overrun",
                       watchdog_.GetTimeout().value());
}

} // namespace cpy6107::framework
