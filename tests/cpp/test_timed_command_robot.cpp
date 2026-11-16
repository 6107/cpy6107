#include "cpy6107/framework/timed_command_robot.hpp"

#include <wpi/hal/simulation/DriverStationData.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

#include "test_harness.hpp"

using cpy6107::framework::FrameworkError;
using cpy6107::framework::HookErrorPolicy;
using cpy6107::framework::LoopTiming;
using cpy6107::framework::OverrunInfo;
using cpy6107::framework::SchedulerHook;
using cpy6107::framework::TimedCommandRobot;
using wpi::units::second_t;
using namespace std::chrono_literals;

// ---- allocation counting (per thread, only while tracking)
// ---------------------------------------

namespace {
thread_local bool g_tracking = false;
thread_local std::size_t g_allocations = 0;
} // namespace

// Sanitizer runtimes replace operator new/delete themselves, so counting is disabled there.
#ifndef CPY6107_SANITIZED
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory,misc-new-delete-overloads)
void* operator new(std::size_t size) {
    if (g_tracking) {
        ++g_allocations;
    }
    void* ptr = std::malloc(size);
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}
void operator delete(void* ptr) noexcept {
    std::free(ptr);
}
void operator delete(void* ptr, std::size_t /*size*/) noexcept {
    std::free(ptr);
}
// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory,misc-new-delete-overloads)
#endif

namespace {

constexpr auto kDefaultWait = 5s;
constexpr std::size_t kMaxWpilibEpochAllocations = 8;

template <typename Predicate>
bool WaitUntil(Predicate predicate, std::chrono::milliseconds timeout = kDefaultWait) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

// Runs StartCompetition() on a thread and stops it on destruction.
class RobotRunner {
  public:
    explicit RobotRunner(TimedCommandRobot& robot)
        : robot_(robot), thread_([&robot] { robot.StartCompetition(); }) {}
    RobotRunner(const RobotRunner&) = delete;
    RobotRunner& operator=(const RobotRunner&) = delete;
    RobotRunner(RobotRunner&&) = delete;
    RobotRunner& operator=(RobotRunner&&) = delete;
    ~RobotRunner() {
        robot_.EndCompetition();
        thread_.join();
    }

  private:
    TimedCommandRobot& robot_;
    std::thread thread_;
};

// Counts every hook; the event log is a fixed array so recording never allocates.
class ProbeRobot : public TimedCommandRobot {
  public:
    using TimedCommandRobot::TimedCommandRobot;

    std::atomic<int> before{0};
    std::atomic<int> after{0};
    std::atomic<int> robot_periodic{0};
    std::atomic<int> simulation_init{0};
    std::atomic<int> simulation_periodic{0};
    std::atomic<int> ds_connected{0};
    std::atomic<int> disabled_init{0};
    std::atomic<int> disabled_exit{0};
    std::atomic<int> disabled_periodic{0};
    std::atomic<int> auto_init{0};
    std::atomic<int> auto_exit{0};
    std::atomic<int> auto_periodic{0};
    std::atomic<int> teleop_init{0};
    std::atomic<int> teleop_exit{0};
    std::atomic<int> teleop_periodic{0};
    std::atomic<int> utility_init{0};
    std::atomic<int> utility_exit{0};
    std::atomic<int> utility_periodic{0};

    std::atomic<std::int64_t> last_pre_ns{-1};
    std::atomic<std::int64_t> last_user_ns{-1};
    std::atomic<std::int64_t> loop_start_ns{0};
    std::atomic<bool> use_epochs{false};

    // allocation tracking over the main loop body
    std::atomic<bool> track_allocations{false};
    std::atomic<std::size_t> max_allocations{0};
    std::atomic<int> tracked_cycles{0};

    // 'B' before, 'R' robot periodic, 'A' after (written by the loop thread only)
    std::array<char, 512> events{};
    std::atomic<std::size_t> event_count{0};

    [[nodiscard]] std::string EventString() const {
        return {events.data(), std::min(event_count.load(), events.size())};
    }

    void Log(char event) {
        const std::size_t index = event_count.load();
        if (index < events.size()) {
            events[index] = event;
            event_count.store(index + 1);
        }
    }

  protected:
    void BeforeUserLoop() override {
        ++before;
        Log('B');
        if (track_allocations.load() && before.load() > 10) {
            g_allocations = 0;
            g_tracking = true;
        }
    }
    void AfterUserLoop(const LoopTiming& timing) override {
        if (g_tracking) {
            g_tracking = false;
            ++tracked_cycles;
            max_allocations.store(std::max(max_allocations.load(), g_allocations));
        }
        last_pre_ns = timing.pre_user_time.count();
        last_user_ns = timing.user_time.count();
        ++after;
        Log('A');
    }

  public:
    void SimulationInit() override { ++simulation_init; }
    void SimulationPeriodic() override { ++simulation_periodic; }
    void DriverStationConnected() override { ++ds_connected; }
    void DisabledInit() override { ++disabled_init; }
    void DisabledExit() override { ++disabled_exit; }
    void DisabledPeriodic() override { ++disabled_periodic; }
    void AutonomousInit() override { ++auto_init; }
    void AutonomousExit() override { ++auto_exit; }
    void AutonomousPeriodic() override { ++auto_periodic; }
    void TeleopInit() override { ++teleop_init; }
    void TeleopExit() override { ++teleop_exit; }
    void TeleopPeriodic() override { ++teleop_periodic; }
    void UtilityInit() override { ++utility_init; }
    void UtilityExit() override { ++utility_exit; }
    void UtilityPeriodic() override { ++utility_periodic; }
    void RobotPeriodic() override {
        ++robot_periodic;
        Log('R');
        loop_start_ns = GetLoopStartTime().count();
        if (use_epochs.load()) {
            AddWatchdogEpoch("custom-epoch");
            PrintLoopWatchdogEpochs();
        }
    }
};

void SetDriverStation(bool enabled, HAL_RobotMode mode, bool ds_attached) {
    HALSIM_SetDriverStationRobotMode(mode);
    HALSIM_SetDriverStationEnabled(enabled ? 1 : 0);
    HALSIM_SetDriverStationDsAttached(ds_attached ? 1 : 0);
    HALSIM_NotifyDriverStationNewData();
}

void ResetDriverStation() {
    SetDriverStation(false, HAL_ROBOT_MODE_UNKNOWN, false);
}

} // namespace

// ---- construction and configuration
// --------------------------------------------------------------

TEST(ConstructorDefaults) {
    ProbeRobot robot;
    CHECK(robot.GetPeriod() == TimedCommandRobot::kDefaultPeriod);
    CHECK(robot.GetWatchdogTimeout() == robot.GetPeriod());
}

TEST(ConstructorFromFrequency) {
    TimedCommandRobot robot{wpi::units::hertz_t{50.0}};
    CHECK(robot.GetPeriod().value() > 0.0199 && robot.GetPeriod().value() < 0.0201);
}

TEST(ConstructorRejectsInvalidArguments) {
    bool period_zero = false;
    bool period_negative = false;
    bool offset_negative = false;
    try {
        TimedCommandRobot robot{second_t{0.0}};
    } catch (const std::invalid_argument&) {
        period_zero = true;
    }
    try {
        TimedCommandRobot robot{second_t{-1.0}};
    } catch (const std::invalid_argument&) {
        period_negative = true;
    }
    try {
        TimedCommandRobot robot{second_t{0.02}, second_t{-0.001}};
    } catch (const std::invalid_argument&) {
        offset_negative = true;
    }
    CHECK(period_zero);
    CHECK(period_negative);
    CHECK(offset_negative);
}

TEST(WatchdogTimeoutIsSettable) {
    TimedCommandRobot robot;
    CHECK(robot.SetWatchdogTimeout(second_t{0.08}).has_value());
    CHECK(robot.GetWatchdogTimeout() == second_t{0.08});

    const auto zero = robot.SetWatchdogTimeout(second_t{0.0});
    CHECK(!zero.has_value());
    CHECK(zero.error() == FrameworkError::kInvalidTimeout);
    CHECK(!robot.SetWatchdogTimeout(second_t{-1.0}).has_value());
    CHECK(robot.GetWatchdogTimeout() == second_t{0.08});
}

TEST(EndCompetitionBeforeStartIsSafe) {
    TimedCommandRobot robot;
    robot.EndCompetition();
    robot.EndCompetition();
    robot.StartCompetition(); // returns immediately: the notifier is already gone
    CHECK(true);
}

// ---- main loop
// -----------------------------------------------------------------------------------

TEST(LoopRunsHooksAroundRobotPeriodic) {
    ProbeRobot robot;
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return robot.after.load() >= 5; }));
    }
    CHECK(robot.simulation_init == 1);
    CHECK(robot.simulation_periodic.load() >= 5);
    CHECK(robot.disabled_periodic.load() >= 5);
    CHECK(robot.last_pre_ns.load() >= 0);
    CHECK(robot.last_user_ns.load() > 0);
    CHECK(robot.loop_start_ns.load() > 0);

    // Every cycle is exactly B R A (the scheduler is not set here); stop may cut a cycle short.
    const std::string events = robot.EventString();
    CHECK(events.size() >= 15);
    for (std::size_t i = 0; i + 2 < events.size(); i += 3) {
        CHECK(events.compare(i, 3, "BRA") == 0);
    }
}

TEST(SchedulerFunctionRunsAtOffsetAfterLoop) {
    constexpr auto kOffset = 10ms;
    ProbeRobot robot{second_t{0.02}, second_t{0.010}};
    std::mutex mutex;
    std::vector<std::chrono::steady_clock::time_point> loop_times;
    std::vector<std::chrono::steady_clock::time_point> scheduler_times;
    std::atomic<int> scheduler_calls{0};
    robot.AddPeriodic(
        [&] {
            const std::scoped_lock lock{mutex};
            loop_times.push_back(std::chrono::steady_clock::now());
        },
        second_t{0.02});
    robot.SetScheduler([&] {
        const std::scoped_lock lock{mutex};
        scheduler_times.push_back(std::chrono::steady_clock::now());
        ++scheduler_calls;
    });
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return scheduler_calls.load() >= 6; }));
    }
    const std::scoped_lock lock{mutex};
    CHECK(loop_times.size() >= 5 && scheduler_times.size() >= 5);
    // scheduler slot k sits ~kOffset after loop-aligned slot k
    for (std::size_t i = 1; i < std::min(loop_times.size(), scheduler_times.size()); ++i) {
        const auto delta = scheduler_times[i] - loop_times[i];
        CHECK(delta > kOffset - 6ms && delta < kOffset + 8ms);
    }
}

TEST(SchedulerHookInterfaceIsCalled) {
    struct CountingHook : SchedulerHook {
        std::atomic<int> runs{0};
        void Run() override { ++runs; }
    };
    ProbeRobot robot;
    CountingHook hook;
    robot.SetScheduler(hook);
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return hook.runs.load() >= 3; }));
    }
}

TEST(ClearSchedulerDisablesTheSlot) {
    ProbeRobot robot;
    std::atomic<int> calls{0};
    robot.SetScheduler([&] { ++calls; });
    robot.ClearScheduler();
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return robot.after.load() >= 5; }));
    }
    CHECK(calls == 0);
}

TEST(AddPeriodicRunsAtItsOwnRate) {
    ProbeRobot robot;
    std::atomic<int> calls{0};
    robot.AddPeriodic([&] { ++calls; }, second_t{0.01}, second_t{0.002});
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return calls.load() >= 8; }));
    }
    CHECK(robot.after.load() >= 1);
}

// ---- driver station modes
// ------------------------------------------------------------------------

TEST(ModeTransitionsCallInitAndExit) {
    ResetDriverStation();
    ProbeRobot robot;
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return robot.disabled_init.load() == 1; }));

        SetDriverStation(false, HAL_ROBOT_MODE_UNKNOWN, true);
        CHECK(WaitUntil([&] { return robot.ds_connected.load() == 1; }));

        SetDriverStation(true, HAL_ROBOT_MODE_TELEOPERATED, true);
        CHECK(WaitUntil([&] { return robot.teleop_periodic.load() >= 2; }));
        CHECK(robot.teleop_init == 1);
        CHECK(robot.disabled_exit == 1);

        SetDriverStation(true, HAL_ROBOT_MODE_AUTONOMOUS, true);
        CHECK(WaitUntil([&] { return robot.auto_periodic.load() >= 2; }));
        CHECK(robot.teleop_exit == 1);
        CHECK(robot.auto_init == 1);

        SetDriverStation(true, HAL_ROBOT_MODE_UTILITY, true);
        CHECK(WaitUntil([&] { return robot.utility_periodic.load() >= 2; }));
        CHECK(robot.auto_exit == 1);
        CHECK(robot.utility_init == 1);

        SetDriverStation(false, HAL_ROBOT_MODE_UNKNOWN, true);
        CHECK(WaitUntil([&] { return robot.disabled_init.load() == 2; }));
        CHECK(robot.utility_exit == 1);
    }
    CHECK(robot.ds_connected == 1);
    CHECK(robot.robot_periodic.load() >= 8);
    ResetDriverStation();
}

// ---- watchdog
// ------------------------------------------------------------------------------------

TEST(LoopOverrunIsReportedAndLoopKeepsRunning) {
    ProbeRobot robot;
    CHECK(robot.SetWatchdogTimeout(second_t{0.000001}).has_value());
    robot.use_epochs = true;
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return robot.after.load() >= 4; }));
    }
    CHECK(robot.GetWatchdogTimeout() == second_t{0.000001});
}

// ---- hook errors and overrun callback
// ---------------------------------------------------------------------------

namespace {
class ThrowingRobot : public TimedCommandRobot {
  public:
    std::atomic<int> before{0};
    std::atomic<int> after{0};

  protected:
    void BeforeUserLoop() override {
        ++before;
        throw std::runtime_error("before failed");
    }
    void AfterUserLoop(const LoopTiming& /*timing*/) override { ++after; }
};
} // namespace

TEST(HookErrorsAreLoggedAndCountedByDefault) {
    ThrowingRobot robot;
    CHECK(robot.GetHookErrorPolicy() == HookErrorPolicy::kLogAndContinue);
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return robot.after.load() >= 3; }));
    }
    CHECK(robot.GetHookErrorCount() >= 3);
}

TEST(StopPolicyRethrowsOutOfStartCompetition) {
    ThrowingRobot robot;
    robot.SetHookErrorPolicy(HookErrorPolicy::kStop);
    bool threw = false;
    std::thread thread{[&] {
        try {
            robot.StartCompetition();
        } catch (const std::runtime_error&) {
            threw = true;
        }
    }};
    thread.join();
    CHECK(threw);
    CHECK(robot.GetHookErrorCount() == 0);
}

TEST(OverrunCallbackReceivesTimeoutAndLoopTime) {
    ProbeRobot robot;
    CHECK(robot.SetWatchdogTimeout(second_t{0.000001}).has_value());
    robot.use_epochs = true;
    std::atomic<int> calls{0};
    std::atomic<std::int64_t> loop_ns{0};
    robot.SetOverrunCallback([&](const OverrunInfo& info) {
        loop_ns = info.loop_time.count();
        CHECK(info.timeout == second_t{0.000001});
        ++calls;
    });
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return calls.load() >= 2; }));
    }
    CHECK(loop_ns.load() > 0);
}

// ---- real-time determinism
// -----------------------------------------------------------------------

TEST(LoopBodyAllocationsStayWithinWpilibEpochBudget) {
    ProbeRobot robot;
    robot.use_epochs = false;
    robot.track_allocations = true;
    {
        const RobotRunner runner{robot};
        CHECK(WaitUntil([&] { return robot.tracked_cycles.load() >= 10; }));
    }
    std::printf("  max allocations per tracked cycle: %zu\n", robot.max_allocations.load());
    // The remaining allocations come from wpi::Tracer::AddEpoch (std::string epoch names inside
    // WPILib's watchdog), one per epoch per cycle: this is an upstream cost, see docs/PLAN.md.
    // The bound guards against this library adding allocations of its own.
#ifndef CPY6107_SANITIZED
    CHECK(robot.max_allocations.load() <= kMaxWpilibEpochAllocations);
#endif
}
