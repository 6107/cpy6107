# Getting started (for other teams)

cpy6107 gives robotpy robots a C++ `TimedCommandRobot` whose loop watchdog timeout you can set, with
hooks that run around every loop cycle (for logging) and a separate slot for the command scheduler.
Use it only if you need that; otherwise `commands2.TimedCommandRobot` is fine.

Status: **alpha**. It tracks one WPILib alpha at a time (currently `2027.0.0a8`), runs on Linux
x86_64, and has not yet been run on a SystemCore. Read [Limitations](#limitations) first.

## 1. Prerequisites

| Need | Details |
| --- | --- |
| Python | 3.14 |
| WPILib / robotpy | `wpilib==2027.0.0a8` (cpy6107 pins exactly this) |
| OS | Linux x86_64 with **glibc >= 2.41** (Debian 13, Ubuntu 25.04+). Ubuntu 24.04 has 2.39: use the Docker image (section 2). Windows, macOS and SystemCore wheels do not exist yet. |
| Source builds only | g++ 14+ (or Clang 19+), CMake >= 3.28, Ninja |

Check your glibc with `ldd --version`.

## 2. Install

The package is not on PyPI yet (see [RELEASING.md](RELEASING.md)). Until it is, build the wheel once
with Docker and install it:

```bash
git clone https://github.com/6107/cpy6107 && cd cpy6107
docker/run.sh docker/build-dist.sh           # writes dist/cpy6107-*.whl
pip install --pre dist/cpy6107-*.whl         # in your robot project's venv (needs glibc >= 2.41)
```

Once it is published this becomes `pip install --pre cpy6107`. Add `cpy6107 == <version>` to your
project's dependencies and keep it in step with your `wpilib` version: cpy6107 versions look like
`2027.0.0a8.post1` (WPILib version, then our release number), and `pip` refuses a mismatch.

Check it:

```bash
python -c "import cpy6107; print(cpy6107.TimedCommandRobot.DEFAULT_PERIOD)"    # 0.02
```

## 3. A minimal robot

```python
import commands2
import wpilib
from cpy6107 import LoopTiming, TimedCommandRobot


class Robot(TimedCommandRobot):
    def __init__(self) -> None:
        super().__init__(period=0.02)                  # seconds
        self.set_watchdog_timeout(0.08)                # report overruns after 80 ms
        self.set_scheduler(commands2.CommandScheduler.get_instance().run)

    def before_user_loop(self) -> None:                # e.g. log inputs
        pass

    def after_user_loop(self, timing: LoopTiming) -> None:   # e.g. log outputs, timing
        pass

    def teleop_periodic(self) -> None:
        pass


if __name__ == "__main__":
    wpilib.run(Robot)
```

A complete, tested version is in [`examples/command_robot/`](../examples/command_robot/). The
hooks, the watchdog and the scheduler slot are explained in the [User Guide](USER-GUIDE.md).

Things that differ from `wpilib.TimedRobot`:

* Method names are the 2027 snake_case names (`robot_periodic`, `teleop_init`, ...). A misspelled
  override is silently never called, so double-check names against `python/cpy6107/_cpy6107.pyi`.
* Times are in seconds.
* It is a `wpilib.IterativeRobotBase` subclass (not `TimedRobot`); `is_enabled()`,
  `is_disabled()`, `is_simulation()` and friends are inherited.
* Do not call `set_scheduler()` **and** run the scheduler yourself in `robot_periodic`.

## 4. Run it in simulation

```bash
python -m robotpy sim            # from your robot project; robotpy-cli ships with wpilib
python -m robotpy test           # run your pytest tests
```

## 5. Deploy to a SystemCore

Not supported yet. cpy6107 is a compiled extension and there is no aarch64 wheel (the SystemCore is
ARM64), so it cannot be installed on a robot today. Tracked in `docs/PLAN-2.md` step 7. Until then,
use cpy6107 in simulation and unit tests only.

## 6. Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| `ImportError: ... libwpilibc.so` or `libwpiHal.so` not found | `wpilib` and `cpy6107` are from different WPILib releases, or `robotpy-native-*` is missing. `pip install --pre "wpilib==<the version cpy6107 pins>"`. |
| `GLIBC_2.41 not found` / pip cannot find `robotpy-native-wpihal` | Your glibc is older than 2.41. Use Debian 13 / Ubuntu 25.04+ or the Docker image. |
| `ERROR: No matching distribution` for cpy6107 | No wheel for your platform/Python; use Python 3.14 on Linux x86_64. |
| My `robot_periodic` / `teleop_periodic` never runs | Misspelled or camelCase name. Use snake_case. |
| "Loop overrun" messages | Your code takes longer than the watchdog timeout. Raise it with `set_watchdog_timeout()` or find the slow code with `add_watchdog_epoch("name")` / `print_watchdog_epochs()`. |
| `ValueError` from the constructor | `period <= 0` or `scheduler_offset < 0`; both are in seconds, not milliseconds. |
| A Python exception in a hook ends `start_competition()` | By design: hook exceptions propagate. Catch them in the hook if the robot must keep running. |
| `isinstance(robot, wpilib.TimedRobot)` is False | Expected: it derives from `IterativeRobotBase`, not `TimedRobot`. |

Still stuck? Open an issue; see [CONTRIBUTING.md](../CONTRIBUTING.md#reporting-a-bug).

## Limitations

* One WPILib alpha at a time (`2027.0.0a8`), exact pin.
* Linux x86_64 only; glibc >= 2.41.
* No SystemCore / aarch64, Windows or macOS wheels yet.
* The robotpy a8 API is itself still changing between alphas.
