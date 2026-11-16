// Measures loop-start jitter of TimedCommandRobot in the simulated HAL.
//
//   cpy6107_jitter_bench [cycles=500] [period_seconds=0.02]
//
// Jitter of cycle n is (start_n - start_0) - n * period, from GetLoopStartTime(). Informational
// only: it never fails, so it is safe to run on shared CI machines. Simulation timing says
// little about SystemCore; use it to compare changes on the same host.
#include "cpy6107/framework/timed_command_robot.hpp"
#include "cpy6107/framework/wpilib_compat.hpp"

#include <wpi/hal/HAL.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>
#include <thread>
#include <vector>

namespace {

namespace chrono = std::chrono;
using cpy6107::framework::LoopTiming;
using cpy6107::framework::TimedCommandRobot;
using wpi::units::second_t;

class BenchRobot : public TimedCommandRobot {
  public:
    BenchRobot(second_t period, std::size_t cycles) : TimedCommandRobot(period), cycles_(cycles) {
        starts_.reserve(cycles);
        user_times_.reserve(cycles);
    }

    [[nodiscard]] const std::vector<chrono::nanoseconds>& Starts() const { return starts_; }
    [[nodiscard]] const std::vector<chrono::nanoseconds>& UserTimes() const { return user_times_; }

  protected:
    void BeforeUserLoop() override {
        if (starts_.size() < cycles_) {
            starts_.push_back(GetLoopStartTime());
        }
    }
    void AfterUserLoop(const LoopTiming& timing) override {
        if (user_times_.size() < cycles_) {
            user_times_.push_back(timing.user_time);
        }
        if (user_times_.size() >= cycles_) {
            EndCompetition();
        }
    }

  private:
    std::size_t cycles_;
    std::vector<chrono::nanoseconds> starts_;
    std::vector<chrono::nanoseconds> user_times_;
};

double Micros(chrono::nanoseconds ns) {
    return static_cast<double>(ns.count()) / 1000.0;
}

void Report(const char* name, std::vector<chrono::nanoseconds> values) {
    std::ranges::sort(values);
    const auto pick = [&](double q) {
        return values[static_cast<std::size_t>(q * static_cast<double>(values.size() - 1))];
    };
    std::println("{:<14} min {:9.1f}  p50 {:9.1f}  p99 {:9.1f}  max {:9.1f}  (us)", name,
                 Micros(values.front()), Micros(pick(0.5)), Micros(pick(0.99)),
                 Micros(values.back()));
}

int Run(int argc, char** argv) {
    const std::size_t cycles = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 500;
    const double period = argc > 2 ? std::strtod(argv[2], nullptr) : 0.02;
    if (cycles < 2 || !(period > 0.0)) {
        std::fputs("usage: cpy6107_jitter_bench [cycles>=2] [period_seconds>0]\n", stderr);
        return 2;
    }
#if CPY6107_WPILIB_ALPHA6
    const bool hal_ok = HAL_Initialize(500, 0) != 0;
#else
    const bool hal_ok = HAL_Initialize() != 0;
#endif
    if (!hal_ok) {
        std::fputs("HAL_Initialize failed\n", stderr);
        return 2;
    }

    BenchRobot robot{second_t{period}, cycles};
    std::thread thread{[&robot] { robot.StartCompetition(); }};
    thread.join();

    const auto& starts = robot.Starts();
    if (starts.size() < 2) {
        std::fputs("no cycles recorded\n", stderr);
        return 1;
    }
    const auto period_ns =
        chrono::duration_cast<chrono::nanoseconds>(chrono::duration<double>{period});
    std::vector<chrono::nanoseconds> jitter;
    jitter.reserve(starts.size());
    for (std::size_t n = 0; n < starts.size(); ++n) {
        const auto ideal = static_cast<std::int64_t>(n) * period_ns;
        const auto delta = starts[n] - starts[0] - ideal;
        jitter.emplace_back(std::abs(delta.count()));
    }
    std::println("cycles {}, period {:.4f} s", starts.size(), period);
    Report("start jitter", std::move(jitter));
    Report("loop body", robot.UserTimes());
    return 0;
}

} // namespace

// The catch handler itself may throw while printing; there is nothing useful left to do then.
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    try {
        return Run(argc, argv);
    } catch (const std::exception& e) {
        std::println(stderr, "error: {}", e.what());
        return 1;
    }
}
