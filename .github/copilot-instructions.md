# GitHub Copilot Repository Instructions (`cpy6107` — FRC 2027)

## Project Purpose & Target Platforms
You are assisting with `cpy6107`, a C++23 robotics library for **FRC 2027**.
- **Robot Runtime Client:** **SystemCore** controller (Raspberry Pi Compute Module 5, 64-bit ARM Cortex-A76, Linux ARM64).
- **Simulation Hosts:** **Ubuntu 24.04** (x64) is the only host built and tested today. Windows 11, macOS and SystemCore framework/Python builds are planned (`docs/PLAN-2.md`).
- **Python:** 3.14 (GIL enabled), exposed through nanobind. WPILib 2027 libraries come from the `robotpy-native` wheels (`wpilib==2027.0.0a6`), not from a WPILib source build.

## Staged Development Workflow
1. Always consult `docs/REQUIREMENTS.md`, `docs/PLAN.md` and `docs/PLAN-2.md` before generating or modifying code.
2. Implement **only one unchecked step** from the active plan (`docs/PLAN-2.md`) per request unless explicitly asked otherwise.
3. After completing a step, update the plan checkboxes and summarize verification results.
4. Keep changes modular, clean, and scoped strictly to the active milestone.

## C++23 & WPILib 2027 Standards
- **Language Standard:** ISO C++23 (`CMAKE_CXX_STANDARD 23`). Do not use compiler-specific extensions.
- **Compiler Warnings:** Code must compile with zero warnings under `-Wall -Wextra -Wpedantic` (GCC/Clang) and `/W4 /permissive-` (MSVC).
- **Namespaces:** Never use `using namespace` in headers; see `.github/instructions/cpp23.instructions.md`.
- **WPILib 2027 Namespaces:** Always use the reorganized `wpi::` C++ namespaces (`wpi::math`, `wpi::units`, `wpi::util`, `wpi::hardware`, `wpi::sim`) instead of legacy `frc::` namespaces.
- **Removed/Deprecated 2027 APIs:** Never use removed 2027 WPILib APIs such as legacy SPI, Relays, Analog Outputs, or `SendableBuilder`. Use Smart IO, CAN, and NetworkTables 4 / `wpi::log` telemetry instead.

## Architecture & Portability Rules
- **Strict Hardware Abstraction:** Separate pure algorithms (`include/cpy6107/core/`, `src/core/`) from hardware interfaces (`include/cpy6107/hal/`, `src/hal/`), the WPILib-based robot framework (`include/cpy6107/framework/`, `src/framework/`) and desktop simulation hooks (`include/cpy6107/sim/`, `src/sim/`); only `core/` and `framework/` exist so far. The nanobind module lives in `bindings/` and no header in `include/` or `src/` may include nanobind or `Python.h`.
- **Zero OS Headers in Core:** Never include `<windows.h>`, `<unistd.h>`, `<sys/socket.h>`, or platform intrinsics in `core/`. Use C++23 standard headers (`<chrono>`, `<span>`, `<array>`, `<expected>`, `<jthread>`) or WPILib `wpi::util` primitives.
- **Real-Time Determinism:** Do not perform dynamic heap allocations (`new`, `std::vector::push_back`, `std::string` allocations) inside periodic `Update()` or control-loop functions.

## Build & Verification Commands
Put the venv on `PATH` first (`export PATH=$PWD/.venv/bin:$PATH`); see `AGENTS.md` and `docs/INSTALL.md`.
- **Configure / build / test (GCC 14):** `cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug --output-on-failure`
- **Same with Clang 19:** replace `linux-debug` with `linux-clang-debug`
- **Python:** `pip install --no-build-isolation -ve . && pytest tests/python`
- **Coverage:** `cmake --preset linux-coverage && cmake --build --preset linux-coverage --target coverage`
- **Cross-compile SystemCore ARM64 (core library only):** `cmake --preset systemcore-arm64 && cmake --build --preset systemcore-arm64`
