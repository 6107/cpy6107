# Toolchain template for FRC 2027 SystemCore (Raspberry Pi Compute Module 5 - ARM64)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Update prefix if using the official WPILib 2027 ARM64 toolchain path
set(TOOLCHAIN_PREFIX aarch64-linux-gnu)

find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

# Tune for Raspberry Pi CM5 (ARM Cortex-A76)
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a76")
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a76")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
