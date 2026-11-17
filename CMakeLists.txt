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

# Presets work without activating the venv: prefer the repository's .venv interpreter.
# CPY6107_PYTHON (environment) overrides it, e.g. the interpreter inside the Docker image.
if(NOT SKBUILD AND NOT Python_EXECUTABLE AND DEFINED ENV{CPY6107_PYTHON})
    set(Python_EXECUTABLE "$ENV{CPY6107_PYTHON}" CACHE FILEPATH "Python interpreter (CPY6107_PYTHON)")
endif()
if(NOT SKBUILD AND NOT Python_EXECUTABLE)
    foreach(_venv_python IN ITEMS .venv/bin/python .venv/Scripts/python.exe)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_venv_python}")
            set(Python_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/${_venv_python}" CACHE FILEPATH
                "Python interpreter (repository .venv)")
            break()
        endif()
    endforeach()
endif()

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

set(CPY6107_SANITIZE "" CACHE STRING "Sanitizers to enable: 'address;undefined' or 'thread' (GCC/Clang, Linux)")
if(CPY6107_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "CPY6107_SANITIZE is not supported with MSVC")
    endif()
    string(REPLACE ";" "," _sanitize_list "${CPY6107_SANITIZE}")
    add_compile_options(-fsanitize=${_sanitize_list} -fno-omit-frame-pointer -fno-sanitize-recover=all -g)
    add_link_options(-fsanitize=${_sanitize_list})
    add_compile_definitions(CPY6107_SANITIZED=1)
endif()

# Older TSan runtimes abort when the kernel's mmap ASLR entropy is high; run tests without ASLR.
set(CPY6107_TEST_LAUNCHER "")
if("thread" IN_LIST CPY6107_SANITIZE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(SETARCH_EXECUTABLE setarch)
    if(SETARCH_EXECUTABLE)
        set(CPY6107_TEST_LAUNCHER "${SETARCH_EXECUTABLE}" "${CMAKE_SYSTEM_PROCESSOR}" -R)
    endif()
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
    if(MSVC)
        # WPILib headers use __VA_OPT__, which needs the conforming preprocessor.
        target_compile_options(cpy6107_framework PUBLIC /Zc:preprocessor /Zc:__cplusplus /utf-8)
    endif()
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
        foreach(_pkg IN LISTS ROBOTPY_NATIVE_PACKAGES)
            list(APPEND _rpath "$ORIGIN/../native/${_pkg}/lib")
        endforeach()
        set_target_properties(_cpy6107 PROPERTIES
            INSTALL_RPATH "${_rpath}"
            BUILD_RPATH "${ROBOTPY_NATIVE_LIB_DIRS}"
        )
    endif()

    if(WIN32)
        # stubgen imports the extension directly, before robotpy has put the WPILib DLLs on the
        # DLL search path, so it cannot load here. Ship the tracked stub instead.
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/python/cpy6107/_cpy6107.pyi" DESTINATION cpy6107)
    else()
        nanobind_add_stub(_cpy6107_stub
            MODULE _cpy6107
            OUTPUT _cpy6107.pyi
            PYTHON_PATH $<TARGET_FILE_DIR:_cpy6107>
            DEPENDS _cpy6107
        )
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_cpy6107.pyi DESTINATION cpy6107)
    endif()

    install(TARGETS _cpy6107 LIBRARY DESTINATION cpy6107)

    # Importable copy of the package built from the current sources: <build>/python/cpy6107.
    # Tests put <build>/python on PYTHONPATH so they never depend on an installed copy.
    if(NOT SKBUILD)
        set(CPY6107_PYTHON_STAGE_DIR "${CMAKE_BINARY_DIR}/python")
        add_custom_target(cpy6107_python_stage ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CPY6107_PYTHON_STAGE_DIR}/cpy6107"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/python/cpy6107/__init__.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/python/cpy6107/_robot.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/python/cpy6107/py.typed"
                $<TARGET_FILE:_cpy6107>
                "${CPY6107_PYTHON_STAGE_DIR}/cpy6107"
            COMMENT "Staging the Python package in ${CPY6107_PYTHON_STAGE_DIR}"
            VERBATIM)
        add_dependencies(cpy6107_python_stage _cpy6107)
    endif()
endif()

# ---- tests -------------------------------------------------------------------------------------

if(CPY6107_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()

    add_executable(cpy6107_tests
        tests/cpp/smoke_test.cpp
    )
    target_link_libraries(cpy6107_tests PRIVATE cpy6107)
    cpy6107_set_warnings(cpy6107_tests)
    add_test(NAME smoke_test COMMAND ${CPY6107_TEST_LAUNCHER} $<TARGET_FILE:cpy6107_tests>)

    if(TARGET cpy6107_python_stage AND Python_EXECUTABLE)
        execute_process(
            COMMAND "${Python_EXECUTABLE}" -c "import pytest, pytest_cov"
            RESULT_VARIABLE _pytest_rc OUTPUT_QUIET ERROR_QUIET)
        if(_pytest_rc EQUAL 0)
            add_test(NAME python_tests
                COMMAND "${Python_EXECUTABLE}" -m pytest "${CMAKE_CURRENT_SOURCE_DIR}/tests/python"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
            set(_python_test_env "CPY6107_STAGE_DIR=${CPY6107_PYTHON_STAGE_DIR};COVERAGE_FILE=${CMAKE_BINARY_DIR}/.coverage")
            if("address" IN_LIST CPY6107_SANITIZE)
                # The interpreter is not instrumented, so the ASan runtime must be preloaded.
                # libstdc++ must come too, or ASan cannot intercept C++ exceptions thrown from it.
                execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libasan.so
                    OUTPUT_VARIABLE _libasan OUTPUT_STRIP_TRAILING_WHITESPACE)
                execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libstdc++.so
                    OUTPUT_VARIABLE _libstdcxx OUTPUT_STRIP_TRAILING_WHITESPACE)
                list(APPEND _python_test_env "LD_PRELOAD=${_libasan} ${_libstdcxx}"
                    "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
            endif()
            set_tests_properties(python_tests PROPERTIES
                ENVIRONMENT "${_python_test_env}"
                TIMEOUT 300)
        else()
            message(WARNING "pytest/pytest-cov missing in ${Python_EXECUTABLE}: 'python_tests' not registered")
        endif()
    endif()

    if(TARGET cpy6107_framework)
        add_executable(cpy6107_framework_tests
            tests/cpp/test_main.cpp
            tests/cpp/test_core.cpp
            tests/cpp/test_timed_command_robot.cpp
        )
        target_link_libraries(cpy6107_framework_tests PRIVATE cpy6107::core cpy6107::framework)
        cpy6107_set_warnings(cpy6107_framework_tests)
        add_test(NAME framework_tests COMMAND ${CPY6107_TEST_LAUNCHER} $<TARGET_FILE:cpy6107_framework_tests>)
        set_tests_properties(framework_tests PROPERTIES TIMEOUT 120)

        add_executable(cpy6107_jitter_bench EXCLUDE_FROM_ALL tests/bench/jitter_bench.cpp)
        target_link_libraries(cpy6107_jitter_bench PRIVATE cpy6107::core cpy6107::framework)
        cpy6107_set_warnings(cpy6107_jitter_bench)
        add_custom_target(bench
            COMMAND ${CPY6107_TEST_LAUNCHER} $<TARGET_FILE:cpy6107_jitter_bench>
            DEPENDS cpy6107_jitter_bench
            COMMENT "Measuring loop-start jitter (informational)"
            USES_TERMINAL)

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
                        --filter ${CMAKE_CURRENT_SOURCE_DIR}/bindings
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

# ---- developer workflow targets: dev, format, lint, check --------------------------------------

if(PROJECT_IS_TOP_LEVEL AND NOT SKBUILD AND NOT CMAKE_CROSSCOMPILING)
    include(DevTools)
endif()
