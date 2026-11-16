cmake_minimum_required(VERSION 3.28)

project(cpy6107
    VERSION 0.1.0
    DESCRIPTION "FRC 2027 SystemCore Runtime and Multi-OS Simulation C++23 Library"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Clang with CMake 3.28 needs clang-scan-deps for module scanning, which we do not use.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(CPY6107_BUILD_TESTS "Build host unit tests" ON)
option(CPY6107_BUILD_FRAMEWORK "Build the WPILib-based framework library (needs robotpy-native)" ON)
option(CPY6107_BUILD_PYTHON "Build the nanobind Python extension" OFF)
option(CPY6107_ENABLE_SIM "Enable desktop simulation support" ON)
option(CPY6107_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CPY6107_ENABLE_COVERAGE "Instrument C++ code with gcov and add a 'coverage' target (GCC)" OFF)
set(CPY6107_COVERAGE_MIN_LINES 70 CACHE STRING "Minimum line coverage (%) enforced by the coverage target")

if(CPY6107_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "CPY6107_ENABLE_COVERAGE requires GCC (gcov-compatible gcovr output)")
    endif()
    add_compile_options(--coverage -O0 -fno-inline)
    add_link_options(--coverage)
endif()

function(cpy6107_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(CPY6107_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(CPY6107_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---- core: pure algorithms, no WPILib ---------------------------------------------------------

add_library(cpy6107
    src/core/version.cpp
)
add_library(cpy6107::core ALIAS cpy6107)

target_include_directories(cpy6107
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
cpy6107_set_warnings(cpy6107)

if(CPY6107_ENABLE_SIM)
    target_compile_definitions(cpy6107 PUBLIC CPY6107_SIMULATION=1)
endif()

# ---- framework: TimedCommandRobot on top of WPILib (robotpy-native) ---------------------------

if(CPY6107_BUILD_FRAMEWORK OR CPY6107_BUILD_PYTHON)
    if(CPY6107_BUILD_PYTHON)
        find_package(Python 3.14 REQUIRED COMPONENTS Interpreter Development.Module)
    else()
        find_package(Python COMPONENTS Interpreter)
    endif()
    find_package(RobotpyNative REQUIRED)

    add_library(cpy6107_framework STATIC
        src/framework/timed_command_robot.cpp
    )
    add_library(cpy6107::framework ALIAS cpy6107_framework)
    target_include_directories(cpy6107_framework
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(cpy6107_framework PUBLIC wpilib::all)
    set_target_properties(cpy6107_framework PROPERTIES POSITION_INDEPENDENT_CODE ON)
    cpy6107_set_warnings(cpy6107_framework)
endif()

# ---- Python extension --------------------------------------------------------------------------

if(CPY6107_BUILD_PYTHON)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
        OUTPUT_VARIABLE nanobind_ROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    find_package(nanobind CONFIG REQUIRED)

    nanobind_add_module(_cpy6107 STABLE_ABI NB_STATIC
        bindings/module.cpp
    )
    target_link_libraries(_cpy6107 PRIVATE cpy6107::framework)
    cpy6107_set_warnings(_cpy6107)

    # The robotpy-native wheels live next to this package in site-packages (…/native/<pkg>/lib).
    if(UNIX AND NOT APPLE)
        set(_rpath)
        foreach(_pkg IN ITEMS wpilib wpihal wpimath wpiutil ntcore wpinet datalog)
            list(APPEND _rpath "$ORIGIN/../native/${_pkg}/lib")
        endforeach()
        set_target_properties(_cpy6107 PROPERTIES
            INSTALL_RPATH "${_rpath}"
            BUILD_RPATH "${ROBOTPY_NATIVE_LIB_DIRS}"
        )
    endif()

    nanobind_add_stub(_cpy6107_stub
        MODULE _cpy6107
        OUTPUT _cpy6107.pyi
        PYTHON_PATH $<TARGET_FILE_DIR:_cpy6107>
        DEPENDS _cpy6107
    )

    install(TARGETS _cpy6107 LIBRARY DESTINATION cpy6107)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_cpy6107.pyi DESTINATION cpy6107)
endif()

# ---- tests -------------------------------------------------------------------------------------

if(CPY6107_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()

    add_executable(cpy6107_tests
        tests/cpp/smoke_test.cpp
    )
    target_link_libraries(cpy6107_tests PRIVATE cpy6107)
    cpy6107_set_warnings(cpy6107_tests)
    add_test(NAME smoke_test COMMAND cpy6107_tests)

    if(TARGET cpy6107_framework)
        add_executable(cpy6107_framework_tests
            tests/cpp/test_main.cpp
            tests/cpp/test_core.cpp
            tests/cpp/test_timed_command_robot.cpp
        )
        target_link_libraries(cpy6107_framework_tests PRIVATE cpy6107::core cpy6107::framework)
        cpy6107_set_warnings(cpy6107_framework_tests)
        add_test(NAME framework_tests COMMAND cpy6107_framework_tests)
        set_tests_properties(framework_tests PROPERTIES TIMEOUT 120)

        if(CPY6107_ENABLE_COVERAGE)
            find_program(GCOVR_EXECUTABLE gcovr HINTS "${Python_ROOT_DIR}/bin" ENV PATH)
            if(GCOVR_EXECUTABLE)
                string(REGEX MATCH "^[0-9]+" _gcc_major "${CMAKE_CXX_COMPILER_VERSION}")
                find_program(GCOV_EXECUTABLE NAMES gcov-${_gcc_major} gcov)
                add_custom_target(coverage
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
                    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                    COMMAND ${GCOVR_EXECUTABLE}
                        --root ${CMAKE_CURRENT_SOURCE_DIR}
                        --gcov-executable ${GCOV_EXECUTABLE}
                        --filter ${CMAKE_CURRENT_SOURCE_DIR}/src/core
                        --filter ${CMAKE_CURRENT_SOURCE_DIR}/src/framework
                        --txt --print-summary
                        --html-details ${CMAKE_BINARY_DIR}/coverage/index.html
                        --fail-under-line ${CPY6107_COVERAGE_MIN_LINES}
                        ${CMAKE_BINARY_DIR}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    DEPENDS cpy6107_tests cpy6107_framework_tests
                    COMMENT "Running tests and checking C++ line coverage"
                    USES_TERMINAL
                )
            else()
                message(WARNING "gcovr not found: 'coverage' target unavailable")
            endif()
        endif()
    endif()
endif()
