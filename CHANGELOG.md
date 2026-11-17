# Changelog

All notable changes to this project are documented here. The version is
`<WPILib version>.postN`: `2027.0.0a8.post1` is built against, and requires, `wpilib==2027.0.0a8`.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2027.0.0a8.post9] - Unreleased

### Changed
- `TimedCommandRobot` is now a Python subclass of `wpilib.IterativeRobotBase` (isinstance checks and
  WPILib APIs work) that delegates the loop to a private C++ engine (`python/cpy6107/_robot.py`).
- Python method names are snake_case (`robot_periodic`, `set_watchdog_timeout`, `before_user_loop`, ...) to match robotpy 2027; the camelCase names are gone.

### Added
- Hook error policy (`HookErrorPolicy`, default `LOG_AND_CONTINUE`) for `before_user_loop`,
  `after_user_loop` and the overrun callback; `get_hook_error_count()`.
- Overrun callback (`set_overrun_callback`, `OverrunInfo`) in C++ and Python.
- `linux-asan` / `linux-tsan` presets (`CPY6107_SANITIZE`) and a `bench` jitter target.
- `TimedCommandRobot`: C++23 timed robot for robotpy with a settable loop watchdog, a separate
  scheduler slot at a configurable offset, `BeforeUserLoop`/`AfterUserLoop` hooks and extra
  periodic callbacks.
- nanobind Python module `cpy6107` with generated type stubs.
- Support for WPILib 2027 alpha-8 (primary) and alpha-6 (development only).
- `RobotBase` state queries (`is_disabled()`, `is_simulation()`, ...) on `TimedCommandRobot`.
- `examples/command_robot`, `docs/LIB_6107_INTEGRATION.md`, `docker/test-lib6107.sh`.
- `docs/GETTING-STARTED.md`, `CONTRIBUTING.md`, `SECURITY.md`, `CODE_OF_CONDUCT.md`, issue forms and a PR template.
- CMake dev targets (`dev`, `format`, `lint`, `check`), workflow presets, Docker build image.
