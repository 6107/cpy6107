# Installation, Toolchain Setup & GitHub Copilot Workflow

## 1. Prerequisites

### Host Simulation Environments
Only **Ubuntu 24.04** is built and tested today; Windows 11 and macOS are planned (`docs/PLAN-2.md`).
* **CMake:** Version 3.28 or newer.
* **Ninja:** Recommended build generator for fast incremental builds across all platforms.
* **C++23 Compiler:**
  * **Windows 11 (planned):** Visual Studio 2026 (or MSVC v144+ toolset with C++23 support).
  * **Ubuntu (24.04+):** GCC 14.x or Clang 19+.
  * **macOS (planned):** Apple Clang (Xcode 16+) or Homebrew LLVM/Clang 19+.
* **IDE:** JetBrains CLion (with the official **GitHub Copilot** plugin installed and signed in).

### SystemCore Target Cross-Compilation (Raspberry Pi CM5 / ARM64)
* To cross-compile for the **SystemCore** runtime controller on Linux/macOS/Windows hosts, install the `aarch64-linux-gnu-gcc` / `aarch64-linux-gnu-g++` (GCC 14+) cross-toolchain or the official WPILib 2027 ARM64 sysroot/toolchain.
* Update `cmake/toolchains/systemcore-arm64.cmake` if your toolchain binaries or sysroot live in a custom directory.

### Python and WPILib (robotpy-native)
The C++ framework and the Python extension build against the WPILib 2027 headers and libraries
shipped in the robotpy wheels (no WPILib download in CMake). Supported: robotpy **2027.0.0a8**
(primary) and **a6**. `robotpy-native-wpihal` a8 is tagged `manylinux_2_41` (its `libMrcLib.so`
imports `sched_setattr@GLIBC_2.41`), so a8 cannot be installed on Ubuntu 24.04 (glibc 2.39); use the
Docker image below or a host with glibc >= 2.41. On Ubuntu 24.04 natively only a6 works.

### Docker (glibc 2.41, Debian trixie): the way to build and test against a8
Needs only Docker. `docker/run.sh` builds the image on first use (Python 3.14, g++-14, clang-19,
CMake, Ninja, the robotpy wheels, ruff, mypy, clang-format/tidy, gcovr), mounts the repository at
`/work` and keeps build output in `build-docker/<wpilib version>/` (separate from the host `build/`).
```bash
docker/run.sh                                           # cmake --workflow --preset linux-ci on a8
docker/run.sh cmake --workflow --preset linux-clang-ci  # any command works
docker/run.sh cmake --workflow --preset linux-coverage-ci
docker/run.sh --wpilib 2027.0.0a6 cmake --workflow --preset linux-ci   # another WPILib release
docker/run.sh bash                                      # interactive shell
docker/run.sh --rebuild bash                            # rebuild the image first
```
On Windows run the same `docker build`/`docker run` from `docker/run.sh` in WSL2 (Linux containers);
the script has not been tried there.

```bash
~/.pyenv/versions/3.14.6/bin/python -m venv .venv      # or: uv venv --python 3.14 .venv
.venv/bin/pip install --pre "wpilib==2027.0.0a6" nanobind scikit-build-core pytest pytest-cov   # a6 on Ubuntu 24.04; a8: use Docker
export PATH=$PWD/.venv/bin:$PATH                          # puts ninja and the venv python first
```

CMake finds the libraries in `.venv/lib/python3.*/site-packages/native` (or the interpreter given
by `-DPython_EXECUTABLE=...`; override with `-DROBOTPY_NATIVE_DIR=...`).

### Build, test and package (Ubuntu 24.04)
```bash
cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug
cmake --preset linux-clang-debug && cmake --build --preset linux-clang-debug   # Clang 19
pip install --no-build-isolation --no-deps -ve .   # editable; --no-deps because the wheel pins wpilib==a8 (needs glibc >= 2.41)
uv build --wheel --no-build-isolation .       # or: python -m build --wheel --no-isolation
```
The build treats warnings as errors (`CPY6107_WARNINGS_AS_ERRORS=ON` in the presets) and generates
`_cpy6107.pyi` with `nanobind_add_stub`. The `systemcore-arm64` preset builds only the core
library until aarch64 WPILib libraries are available.

### One command: build, lint, test
```bash
cmake --workflow --preset linux-ci            # configure -> build -> lint -> ctest (C++ and pytest)
cmake --workflow --preset linux-clang-ci      # same with Clang 19
cmake --workflow --preset linux-coverage-ci   # coverage gate (C++ >= 70%)
cmake --build --preset linux-debug-check      # on an existing build tree
cmake --build --preset linux-debug-format     # reformat C++ and Python in place
```
| Target | Does |
|---|---|
| `dev` | builds core, framework, C++ tests, Python extension, stub and staged package |
| `format` | `clang-format -i` + `ruff format` |
| `lint-cpp` / `lint-py` / `lint` | clang-format `--dry-run --Werror` + clang-tidy / ruff + mypy / both |
| `check` | `dev` + `lint`, then `ctest` (includes `python_tests`, run against `build/<preset>/python`, not an installed copy) |

The repository `.venv` is used automatically for Python and the tools (clang-format and clang-tidy
are pip packages); activate it (or put `.venv/bin` on `PATH`) so CMake also finds `ninja`.

Windows (PowerShell). The sdist builds with MSVC (`uv sync`); `cmake/FindRobotpyNative.cmake` uses
the wheels' `.dll`/`.lib` pairs. Runtime verification is still in progress:
```powershell
.venv\Scripts\Activate.ps1
cmake --workflow --preset windows-ci          # from a Developer PowerShell (MSVC, /W4 /WX)
cmake --build --preset windows-debug --target check
```

### Tests and coverage
```bash
ctest --preset linux-debug --output-on-failure     # smoke_test + framework_tests (simulated HAL)
pytest tests/python                                # also enforces >= 70% Python line coverage
cmake --preset linux-coverage && cmake --build --preset linux-coverage
cmake --build --preset linux-coverage --target coverage   # runs ctest, enforces >= 70% C++ lines
```
The `linux-coverage` preset also builds the Python extension, so pytest exercises
`bindings/module.cpp` and the C++ report includes it (about 88%; the unhit lines are mostly the
autonomous/teleop/utility virtual overrides). The report is written to
`build/linux-coverage/coverage/index.html`.

### Sanitizers and benchmark
```bash
cmake --workflow --preset linux-asan-ci    # AddressSanitizer + UBSan, C++ and Python tests
cmake --workflow --preset linux-tsan-ci    # ThreadSanitizer, C++ tests only
cmake --build --preset linux-debug --target bench   # loop-start jitter, informational only
```
`CPY6107_SANITIZE` (`address;undefined` or `thread`) selects the sanitizers. Under ASan the Python
tests preload `libasan` and `libstdc++` automatically. TSan runs the tests through `setarch -R`
(no ASLR); in Docker that needs `CPY6107_DOCKER_ARGS="--security-opt seccomp=unconfined"` before
`docker/run.sh`. The TSan preset does not build the Python extension (an uninstrumented
interpreter makes TSan reports unreliable). The allocation-count test is skipped under sanitizers.
`framework_tests` uses a dependency-free harness (`tests/cpp/test_harness.hpp`) and drives
the simulated driver station through `HALSIM_*` calls.

## 2. Setting Up the Project in CLion

1. **Open in CLion:** choose **Open**, select the repository directory, and enable the `linux-debug` (and `linux-clang-debug`) CMake presets. Put the venv's `bin` first on `PATH` (or set `-DPython_EXECUTABLE=.../.venv/bin/python`) so CMake finds the robotpy-native libraries.
2. **Verify:** run `cpy6107_tests` / `framework_tests`, or from a terminal:
   ```bash
   cmake --preset linux-debug
   cmake --build --preset linux-debug
   ctest --preset linux-debug --output-on-failure
   ```

## 3. Working in Stages with GitHub Copilot

GitHub Copilot automatically reads `.github/copilot-instructions.md` and `.github/instructions/*.instructions.md` in every chat and edit session.

1. **Start Clean per Milestone:** Create a feature branch or clean commit before starting each step in `docs/PLAN-2.md`.
2. **Prompt One Phase at a Time:** Open GitHub Copilot Chat (Agent / Edit mode) in CLion and attach `#file:docs/REQUIREMENTS.md` and `#file:docs/PLAN-2.md` with a prompt such as:
   > "Read `docs/REQUIREMENTS.md` and `docs/PLAN-2.md`. Implement only the next unchecked step in `docs/PLAN-2.md`. Build `linux-debug` and run `ctest --preset linux-debug` to verify."
3. **Review, Check Off & Commit:** Inspect the diff in CLion's Git tool window, verify the plan checkboxes are updated, commit your changes, and start a fresh Copilot chat thread for the next phase.

## 4. VS Code Users (Untested)
If a contributor opens this repository in Visual Studio Code, `.vscode/settings.json`, `.vscode/tasks.json`, and `.vscode/extensions.json` provide initial CMake Presets and GitHub Copilot recommendations. These files are untested because CLion is the primary development environment.
