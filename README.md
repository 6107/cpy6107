# cpy6107
C/C++ specialized classes for the CyberJagzz 6107 FRC Robot focused on providing C/C++ where needed in our robotpy robots.

Feel free to use anything in this project that you find useful.

For this first release, the main deliverable is `cpy6107.TimedCommandRobot`: a C++23 timed robot for 
robotpy 2027 whose loop watchdog timeout can be set, with hooks around every loop cycle (for logging) 
and a separate slot for the command scheduler. 

Future releases are expected to focus on other areas where:
 - python may do a fair amount of processing before calling into wpilib (or other C/C++ libraries),
 - more computationally intensive areas such as vision, path planning/optimization, and machine learning.

Status: **alpha**, tracks WPILib `2027.0.0a8`, Linux x86_64 (glibc >= 2.41).

- **Robot runtime target:** **SystemCore** (Raspberry Pi CM5, ARM64); not yet supported by the Python package.
- **Simulation targets:** Ubuntu Linux (tested); Windows 11 and macOS are planned.
- **Automation:** GitHub Copilot instructions (`.github/copilot-instructions.md`, `.github/instructions/`) and GitHub Actions (`.github/workflows/`).

## Documentation

| I want to... | Read |
| --- | --- |
| Use it in my team's robot | [docs/GETTING-STARTED.md](docs/GETTING-STARTED.md), then [docs/USER-GUIDE.md](docs/USER-GUIDE.md) |
| See a working robot | [examples/command_robot/](examples/command_robot/) |
| Build, lint and test it (Linux, Docker, Windows) | [docs/INSTALL.md](docs/INSTALL.md) |
| Report a bug or ask for a change | see [Support](#support-and-bug-reports) |
| Contribute code | [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) |
| Report a security problem | [SECURITY.md](SECURITY.md) |
| Cut a release (maintainers) | [docs/RELEASING.md](docs/RELEASING.md), [CHANGELOG.md](CHANGELOG.md) |
| Understand the design and plans | [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md), [docs/PLAN.md](docs/PLAN.md), [docs/PLAN-2.md](docs/PLAN-2.md), [docs/RESEARCH-NOTES.md](docs/RESEARCH-NOTES.md), [docs/TODO.md](docs/TODO.md) |

## Support and bug reports

* **Something broke or is confusing:** check the
  [troubleshooting table](docs/GETTING-STARTED.md#6-troubleshooting), then
  [open a bug report](https://github.com/6107/cpy6107/issues/new/choose). The form asks for versions,
  OS/glibc and a minimal robot.
* **You need something changed or added:** open a *Change request* from the same page.
* **Security:** use the private report link in [SECURITY.md](SECURITY.md), not a public issue.
* This is a volunteer, student-run project: we answer as time allows.

## IDE Setup & Support Notes

* **Primary IDE (CLion + GitHub Copilot):** This repository is structured and verified for JetBrains CLion using `CMakePresets.json` and the GitHub Copilot plugin.
* **Secondary IDE (VS Code - Untested):** Seed configuration files are provided under `.vscode/` (`settings.json`, `tasks.json`, `extensions.json`) for contributors who prefer Visual Studio Code. **Note:** The `.vscode/` configurations are untested starter templates, as CLion is the initial target IDE.

## Installation & Quick Start

See [docs/INSTALL.md](docs/INSTALL.md) for full toolchain prerequisites, CLion setup, cross-compilation setup, and GitHub Copilot staged workflow instructions.

1. Create the Python 3.14 venv with the robotpy-native wheels (see `docs/INSTALL.md`).
2. Open the folder in **CLion** and select the `linux-debug` CMake preset (or use the command line).
3. Build the project and run `ctest --preset linux-debug --output-on-failure` to verify your local simulation environment.
4. Open **GitHub Copilot Chat** in CLion and instruct Copilot to read `docs/REQUIREMENTS.md` and `docs/PLAN-2.md`, then implement the next unchecked step.

## Repository Structure

* `.github/copilot-instructions.md`: Always-on repository instructions for GitHub Copilot (C++23, WPILib 2027 `wpi::` rules, SystemCore vs. Simulation boundaries).
* `.github/instructions/cpp-core.instructions.md`: Scoped Copilot rules enforcing zero OS headers and zero steady-state heap allocations in `core/`.
* `.github/workflows/ci.yml`: GitHub Actions CI workflow building and testing Ubuntu (GCC 14 and Clang 19), coverage, the Python package and SystemCore ARM64 cross-compilation of the core library.
* `.github/ISSUE_TEMPLATE/`, `.github/PULL_REQUEST_TEMPLATE.md`: issue forms and the PR checklist.
* `docs/INSTALL.md`: Detailed setup, prerequisites, and staged GitHub Copilot usage guide.
* `docs/REQUIREMENTS.md`: Functional and non-functional requirements for SystemCore runtime and multi-OS simulation.
* `docs/PLAN.md`: Completed initial plan (steps 1–7). `docs/PLAN-2.md`: next-phase plan. `docs/TODO.md`: background and future ideas. `docs/USER-GUIDE.md`: how to use `TimedCommandRobot` from Python.
* `CMakeLists.txt` & `CMakePresets.json`: C++23 build configuration supporting host simulation and ARM64 cross-compilation.
* `cmake/toolchains/systemcore-arm64.cmake`: CMake toolchain template for targeting SystemCore (ARM64 Cortex-A76).
* `.vscode/`: Untested starter configuration files for VS Code users.

