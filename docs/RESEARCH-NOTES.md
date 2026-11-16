# Research Notes (Step 1)

Sources: `../allwpilib` (HEAD `6e9a27c1bc`, 2026-11-14), `../AdvantageKit/akit`, `../lib_6107`,
robotpy 2027.0.0a6 installed in `.venv`.

## 1. WPILib C++ framework
- `wpi::TimedRobot : IterativeRobotBase`. The constructor registers `LoopFunc` as a periodic callback in a
  `PeriodicPriorityQueue`, creates a HAL notifier and `StartCompetition()` loops on `RunCallbacks()`.
  `AddPeriodic(callback, period, offset)` schedules extra callbacks on the same notifier.
- `RunCallbacks()` pops the earliest callback, `HAL_SetNotifierAlarm`, blocks in `WPI_WaitForObject`,
  runs the callback, then reschedules by whole periods (skips missed slots after an overrun).
- `IterativeRobotBase::LoopFunc()` — refresh DS data, `m_watchdog.Reset()`, detect mode change
  (exit/init), run the mode periodic, `RobotPeriodic()`, tunables update, `SimulationPeriodic()` (sim),
  `m_watchdog.Disable()`, flush NT, print epochs if expired.
- `m_watchdog` is **private** in `IterativeRobotBase` (default timeout = loop period). `Watchdog` itself is
  a public class with `SetTimeout/GetTimeout/Reset/Disable/AddEpoch/PrintEpochs/IsExpired`.
- Notifier = HAL alarm object; wait via `WPI_WaitForObject`; destroying it wakes the waiter, which is how
  `EndCompetition()` stops the loop.

## 2. AdvantageKit `LoggedRobot` (Java)
- Derives `IterativeRobotBase`, owns its own notifier and loop in `startCompetition()`; no `addPeriodic`
  support ("custom periodic callbacks are not supported").
- Loop: wait for notifier at `nextCycleNs` (skip wait if already late), then
  `Logger.periodicBeforeUser()` -> `loopFunc()` -> `Logger.periodicAfterUser(userTime, beforeTime)`.
- **Watchdog is obtained by reflection** on `m_watchdog` and `setTimeout(period * 4)` is applied. Exposes
  `setLoopOverrunWarningTimeout()` and `setUseTiming()`. Same problem we have, solved with reflection.

## 3. lib_6107 `LoggedRobot` (Python)
- Derives `IterativeRobotBase`, reimplements the loop with `hal.create_notifier` / `wait_for_object`,
  microsecond timing, and the same Logger before/after hooks as akit.
- It creates `self.watchdog = Watchdog(...)`, but this is **not** the base class's watchdog, so setting its
  timeout has no effect on the overrun detection used by `_loop_func()`. This is the drawback to fix.
- No `add_periodic` support, so it cannot be combined with `TimedCommandRobot`.
- `Logger.periodic_after_user` exceptions are suppressed (FMS workaround).

## 4. robotpy `commands2.TimedCommandRobot`
```python
class TimedCommandRobot(TimedRobot):
    SCHEDULER_OFFSET = 0.005
    def __init__(self, period=TimedRobot.DEFAULT_PERIOD / 1000):   # robotpy DEFAULT_PERIOD is 20.0 (ms)
        super().__init__(period)
        self.addPeriodic(CommandScheduler.getInstance().run, period, self.SCHEDULER_OFFSET)
```
No C++ `TimedCommandRobot` exists in `../allwpilib` (Python-only construct). Commands V3 is not yet in robotpy.

## 5. Findings that affect design
1. **Version skew.** robotpy `2027.0.0a6` (what lib_6107 runs) differs from `../allwpilib` HEAD:
   microsecond vs nanosecond time types (`TimedRobot::GetLoopStartTime`, `Watchdog`, `m_startTime`),
   `PrintLoopOverrunMessage()` vs `wpi::util::Alert m_loopOverrunAlert`, no `TunableRegistry` in `LoopFunc`.
   PyPI also has `a7` and `a8`. Our class layout/ABI **must** match the runtime `robotpy-native-wpilib`.
   A copied `LoopFunc` must be version-guarded, and units isolated behind a small adapter.
2. **pybind11 vs nanobind.** robotpy classes are semiwrap/pybind11. A nanobind type cannot inherit from
   `wpilib.TimedRobot` (layout conflict). `wpilib._impl.start` only needs the robot object to be
   constructible, with `startCompetition()` and `endCompetition()` — so a standalone nanobind robot may
   work. Needs the step 3a spike.
3. **Native libs** in `.venv/.../native/*/lib` need RPATH or `LD_LIBRARY_PATH`; `libwpilibc.so` has no
   RPATH to siblings.
4. `IterativeRobotBase` virtuals are exposed through robotpy (`robotPeriodic`, ...), but `LoopFunc` is
   protected; Python subclasses of a nanobind robot need trampolines for these virtuals.
5. **glibc.** robotpy-native a6.post4/a7/a8 Linux wheels are `manylinux_2_41`; a6 is `manylinux_2_39`.
   This host (Ubuntu 24.04, glibc 2.39) can only run a6. Initial decision: target a6, adapter-based.
   Update: only `robotpy-native-wpihal` is `manylinux_2_41` (libMrcLib needs `sched_setattr@GLIBC_2.41`);
   a8 is now supported through the Docker image (`docker/run.sh`, Debian trixie, glibc 2.41).
6. `.venv` mirrors lib_6107's package set; installed with `--no-deps` for `wpiutil`/`wpimath`
   because `robotpy-native-tunables a8` requires `wpiutil a8` (alpha metadata mismatch in lib_6107 too).
