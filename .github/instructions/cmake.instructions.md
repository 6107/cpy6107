---
applyTo: "**/CMakeLists.txt,cmake/*.cmake"
---
# CMake & WPILib Build Rules
* Minimum CMake version is `3.28` (required for full C++23 and `nanobind` support).
* Never use `file(GLOB ...)` for source files; list `.cpp` files explicitly so CMake detects new files automatically.
* Link WPILib targets using `target_link_libraries(<target> PRIVATE wpilib::wpimath wpilib::ntcore wpilib::wpiutil)`.
* Use `nanobind_add_module(<module_name> STABLE_ABI NB_STATIC ...)` when targeting Python 3.12+ Limited API wheels.
