# AGENTS.md — Autonomous Coding Agent Workflow

## Environment Setup & Build Commands
This repository uses `CMakePresets.json` and `scikit-build-core`. Always verify changes by building both the C++ test suite and the Python extension.

WPILib comes from the `robotpy-native` wheels (a8 primary via `docker/run.sh`, which provides glibc 2.41; the Ubuntu 24.04 host `.venv` can only install a6), not from source. Create a Python 3.14 venv first (see `docs/INSTALL.md`) and put it on `PATH`:
```bash
export PATH=$PWD/.venv/bin:$PATH   # CMake finds the native libraries via this Python
```

### Docker (a8, glibc 2.41) — use this when the host glibc is < 2.41
```bash
docker/run.sh                                           # cmake --workflow --preset linux-ci
docker/run.sh --wpilib 2027.0.0a6 cmake --workflow --preset linux-clang-ci
```
Build output goes to `build-docker/<version>/`. Both a6 and a8 must pass before finishing a change that touches `src/framework`, `bindings` or `cmake`. After editing `LoopBody()`, run `python scripts/check_loopfunc.py <allwpilib tag>`.

### Linux (Ubuntu 24.04+)
* **Everything (configure, build, lint, C++ + Python tests) in one command:**
  ```bash
  cmake --workflow --preset linux-ci          # or linux-clang-ci
  cmake --build --preset linux-debug-check    # same, on an already configured tree
  ```
  Individual targets: `dev` (build all), `format`, `lint`, `lint-cpp`, `lint-py`, `check` (dev + lint + ctest, which includes the pytest suite as `python_tests`). Presets: `linux-debug-{dev,lint,check,format}`, `linux-clang-debug-*`.
* **Configure & Build C++ Tests (GCC 14+):**
  ```bash
  cmake --preset linux-debug
  cmake --build --preset linux-debug
  ctest --preset linux-debug --output-on-failure
  ```
* **Configure & Build C++ Tests (Clang 19+):**
  ```bash
  cmake --preset linux-clang-debug
  cmake --build --preset linux-clang-debug
  ctest --preset linux-clang-debug --output-on-failure
  ```
* **Build & Test Python Package:**
  ```bash
  pip install --no-build-isolation --no-deps -ve .   # --no-deps: host wpilib may differ from the a8 pin
  pytest tests/python      # enforces >= 70% coverage
  ```
* **C++ coverage (GCC only):**
  ```bash
  cmake --preset linux-coverage && cmake --build --preset linux-coverage
  cmake --build --preset linux-coverage --target coverage
  ```

### Windows 11 and macOS
Not supported yet: there are no `windows-*` or `macos-*` presets (see `docs/PLAN-2.md`, steps 2d and 7b).
Do not document or invent commands for them until the presets exist.

## Agent Verification Checklist
Before completing any task or submitting a PR, you MUST verify:
1. **Zero Warning Policy:** All C++23 targets compile cleanly under `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang) and `/W4 /WX` (MSVC, once a Windows preset exists).
2. **Formatting & Linting:** C++ passes `clang-format --dry-run --Werror` and `clang-tidy`; Python passes `ruff check`, `ruff format --check`, and `mypy`.
3. **Header Hygiene:** No file in `include/` or `src/` includes `<nanobind/nanobind.h>` or `<Python.h>`.
4. **Stub Regeneration:** Any changes to `bindings/module.cpp` compile cleanly and regenerate updated `.pyi` stubs without errors.
5. **Tests Pass:** Both `ctest` and `pytest` exit with status code `0`.
6. **Coverage:** `cmake --build --preset linux-coverage --target coverage` and `pytest` (which enforces `--cov-fail-under=70`) both succeed.
