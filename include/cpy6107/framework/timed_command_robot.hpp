#pragma once

#include "cpy6107/framework/wpilib_compat.hpp"

#include <wpi/framework/IterativeRobotBase.hpp>
#include <wpi/hal/Types.h>
#include <wpi/internal/PeriodicPriorityQueue.hpp>
#include <wpi/system/Watchdog.hpp>
#include <wpi/units/frequency.hpp>
#include <wpi/units/time.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <string_view>

namespace cpy6107::framework {

enum class FrameworkError : std::uint8_t {
    kInvalidTimeout,
};

// Time spent in the two phases of one main-loop cycle, passed to TimedCommandRobot::AfterUserLoop.
struct LoopTiming {
    std::chrono::nanoseconds pre_user_time{}; // BeforeUserLoop()
    std::chrono::nanoseconds user_time{};     // mode periodic + RobotPeriodic + sim periodic
};

// Pure C++ alternative to a std::function for the command scheduler: implement Run() to call
// the scheduler's run method.
class SchedulerHook {
  public:
    SchedulerHook() = default;
    SchedulerHook(const SchedulerHook&) = delete;
    SchedulerHook& operator=(const SchedulerHook&) = delete;
    SchedulerHook(SchedulerHook&&) = delete;
    SchedulerHook& operator=(SchedulerHook&&) = delete;
    virtual ~SchedulerHook() = default;

    virtual void Run() = 0;
};

// C++ counterpart of robotpy's commands2.TimedCommandRobot, built on wpi::IterativeRobotBase.
//
// Unlike wpi::TimedRobot, the loop is owned by this class (a private wpi::Watchdog drives overrun
// detection), which is what makes the watchdog timeout settable. wpi::TimedRobot cannot be used as
// a base: it registers IterativeRobotBase::LoopFunc() (non-virtual, using the base's private
// watchdog) in its own private callback queue.
//
// Each period one slot runs the robot loop (BeforeUserLoop -> mode/robot periodic ->
// AfterUserLoop); a second slot, offset by the scheduler offset, runs the command scheduler.
//
// Threading: configure (SetScheduler, AddPeriodic, SetWatchdogTimeout) before StartCompetition().
// EndCompetition() may be called from any thread.
// Allocation: nothing in the periodic path allocates except Watchdog::AddEpoch on its first use
// of each epoch name (same as wpi::TimedRobot).
class TimedCommandRobot : public wpi::IterativeRobotBase {
  public:
    static constexpr wpi::units::second_t kDefaultPeriod{0.02};
    static constexpr wpi::units::second_t kSchedulerOffset{0.005};

    // Throws std::invalid_argument if period <= 0 or scheduler_offset < 0.
    explicit TimedCommandRobot(wpi::units::second_t period = kDefaultPeriod,
                               wpi::units::second_t scheduler_offset = kSchedulerOffset);
    explicit TimedCommandRobot(wpi::units::hertz_t frequency);

    TimedCommandRobot(const TimedCommandRobot&) = delete;
    TimedCommandRobot& operator=(const TimedCommandRobot&) = delete;
    TimedCommandRobot(TimedCommandRobot&&) = delete;
    TimedCommandRobot& operator=(TimedCommandRobot&&) = delete;
    ~TimedCommandRobot() override;

    void StartCompetition() override;
    void EndCompetition() override;

    // Runs `callback` every `period`, `offset` after the common start time, on the robot thread.
    void AddPeriodic(std::function<void()> callback, wpi::units::second_t period,
                     wpi::units::second_t offset = wpi::units::second_t{0.0});

    // Command scheduler invoked once per period at the scheduler offset. The std::function and
    // reference overloads replace each other; the referenced hook must outlive the robot.
    void SetScheduler(std::function<void()> scheduler);
    void SetScheduler(SchedulerHook& hook);
    void ClearScheduler();

    // Loop overrun threshold. Defaults to the loop period.
    [[nodiscard]] std::expected<void, FrameworkError>
    SetWatchdogTimeout(wpi::units::second_t timeout);
    [[nodiscard]] wpi::units::second_t GetWatchdogTimeout() const;

    // Adds a named epoch to the current cycle (valid inside the loop body and its hooks).
    void AddWatchdogEpoch(std::string_view name);
    // IterativeRobotBase::PrintWatchdogEpochs() prints the base's unused watchdog; use this one.
    void PrintLoopWatchdogEpochs();

    // Monotonic time at the start of the current periodic callback.
    [[nodiscard]] std::chrono::nanoseconds GetLoopStartTime() const;

  protected:
    // Run outside the watchdog window, immediately before / after the loop body.
    virtual void BeforeUserLoop();
    virtual void AfterUserLoop(const LoopTiming& timing);

  private:
    void ExecuteLoopCycle();
    void LoopBody();
    void ReportLoopOverrun() const;

    wpi::internal::PeriodicPriorityQueue callbacks_;
    std::atomic<HAL_NotifierHandle> notifier_{HAL_INVALID_HANDLE};
    compat::MonotonicDuration start_time_{};
    wpi::Watchdog watchdog_;
    std::function<void()> scheduler_;
    int last_mode_ = -1;
    bool called_ds_connected_ = false;
};

} // namespace cpy6107::framework
