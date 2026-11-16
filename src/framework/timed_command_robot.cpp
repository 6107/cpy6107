#include "cpy6107/framework/timed_command_robot.hpp"

#include <wpi/driverstation/RobotState.hpp>
#include <wpi/driverstation/internal/DriverStationBackend.hpp>
#include <wpi/hal/DriverStation.h>
#include <wpi/hal/DriverStation.hpp>
#include <wpi/hal/DriverStationTypes.h>
#include <wpi/hal/Notifier.hpp>
#include <wpi/hal/UsageReporting.hpp>
#include <wpi/nt/NetworkTableInstance.hpp>
#include <wpi/smartdashboard/SmartDashboard.hpp>
#include <wpi/system/Errors.hpp>

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace cpy6107::framework {

// File-local shorthands; never put these in a header.
namespace chrono = std::chrono;
using wpi::RobotMode;
using wpi::units::hertz_t;
using wpi::units::second_t;

namespace {

// ---- release-specific (WPILib 2027 alpha-6): see wpilib_compat.hpp ---------------------------

void UpdateTelemetry(wpi::Watchdog& watchdog) {
    wpi::SmartDashboard::UpdateValues();
    watchdog.AddEpoch("SmartDashboard::UpdateValues()");
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

void TimedCommandRobot::ExecuteLoopCycle() {
    const auto before_start = compat::MonotonicNow();
    BeforeUserLoop();
    const auto user_start = compat::MonotonicNow();
    LoopBody();
    const auto user_end = compat::MonotonicNow();
    AfterUserLoop(
        LoopTiming{.pre_user_time = user_start - before_start, .user_time = user_end - user_start});
}

// Mirrors wpi::IterativeRobotBase::LoopFunc() (alpha-6) with watchdog_ in place of the private
// base watchdog. Keep in sync with upstream when raising the supported WPILib release.
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
