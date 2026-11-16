# FindRobotpyNative.cmake
#
# Locates the WPILib 2027 C++ headers and shared libraries shipped by the robotpy-native-*
# wheels (site-packages/native/<package>/{include,lib}) and exposes them as IMPORTED targets:
#   wpilib::wpilibc  wpilib::hal  wpilib::wpimath  wpilib::wpiutil
#   wpilib::ntcore   wpilib::wpinet  wpilib::datalog
# plus the aggregate wpilib::all, which links every library directly so that they are all
# DT_NEEDED (the robotpy libraries only carry an $ORIGIN RUNPATH, so siblings in other package
# directories are found only if the consumer lists them itself).
#
# Search order for the `native` directory:
#   1. ROBOTPY_NATIVE_DIR (cache variable)
#   2. <source dir>/.venv/lib/python*/site-packages/native
#   3. the site-packages of the Python interpreter found by find_package(Python)
#
# Result variables: RobotpyNative_FOUND, ROBOTPY_NATIVE_DIR, RobotpyNative_VERSION (0 if unknown)

include_guard(GLOBAL)

set(ROBOTPY_NATIVE_DIR "" CACHE PATH "site-packages/native directory from robotpy-native wheels")

function(_robotpy_native_is_valid dir out_var)
    if(EXISTS "${dir}/wpilib/include" AND EXISTS "${dir}/wpilib/lib")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

if(NOT ROBOTPY_NATIVE_DIR)
    set(_candidates)
    file(GLOB _venv_candidates "${CMAKE_SOURCE_DIR}/.venv/lib/python*/site-packages/native")
    list(APPEND _candidates ${_venv_candidates})

    if(Python_EXECUTABLE)
        set(_python "${Python_EXECUTABLE}")
    elseif(Python3_EXECUTABLE)
        set(_python "${Python3_EXECUTABLE}")
    endif()
    if(_python)
        execute_process(
            COMMAND "${_python}" -c "import sysconfig; print(sysconfig.get_paths()['purelib'])"
            OUTPUT_VARIABLE _purelib
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _rc
        )
        if(_rc EQUAL 0)
            list(APPEND _candidates "${_purelib}/native")
        endif()
    endif()

    foreach(_dir IN LISTS _candidates)
        _robotpy_native_is_valid("${_dir}" _ok)
        if(_ok)
            set(ROBOTPY_NATIVE_DIR "${_dir}" CACHE PATH "" FORCE)
            break()
        endif()
    endforeach()
endif()

_robotpy_native_is_valid("${ROBOTPY_NATIVE_DIR}" _native_ok)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RobotpyNative
    REQUIRED_VARS ROBOTPY_NATIVE_DIR _native_ok
    FAIL_MESSAGE "robotpy-native WPILib not found. Install the robotpy wheels into a venv (uv pip install --prerelease=allow wpilib==2027.0.0a6) or set ROBOTPY_NATIVE_DIR."
)

if(NOT RobotpyNative_FOUND)
    return()
endif()

# <target>  <package dir>  <library file stem>
set(_robotpy_libs
    "wpilibc|wpilib|wpilibc"
    "hal|wpihal|wpiHal"
    "wpimath|wpimath|wpimath"
    "wpiutil|wpiutil|wpiutil"
    "ntcore|ntcore|ntcore"
    "wpinet|wpinet|wpinet"
    "datalog|datalog|datalog"
)

set(_all_targets)
set(ROBOTPY_NATIVE_LIB_DIRS)
foreach(_entry IN LISTS _robotpy_libs)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _target)
    list(GET _parts 1 _pkg)
    list(GET _parts 2 _stem)

    set(_lib "${ROBOTPY_NATIVE_DIR}/${_pkg}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${_stem}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    if(NOT EXISTS "${_lib}")
        message(FATAL_ERROR "RobotpyNative: expected library not found: ${_lib}")
    endif()

    if(NOT TARGET wpilib::${_target})
        add_library(wpilib::${_target} SHARED IMPORTED GLOBAL)
        set_target_properties(wpilib::${_target} PROPERTIES
            IMPORTED_LOCATION "${_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${ROBOTPY_NATIVE_DIR}/${_pkg}/include"
        )
        # Headers of the robotpy wheels are third-party: keep them out of warning/clang-tidy scope.
        set_property(TARGET wpilib::${_target} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
            "${ROBOTPY_NATIVE_DIR}/${_pkg}/include")
    endif()
    list(APPEND _all_targets wpilib::${_target})
    list(APPEND ROBOTPY_NATIVE_LIB_DIRS "${ROBOTPY_NATIVE_DIR}/${_pkg}/lib")
endforeach()

if(NOT TARGET wpilib::all)
    add_library(wpilib::all INTERFACE IMPORTED GLOBAL)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_property(TARGET wpilib::all PROPERTY INTERFACE_LINK_LIBRARIES
            "-Wl,--no-as-needed" ${_all_targets} "-Wl,--as-needed")
    else()
        set_property(TARGET wpilib::all PROPERTY INTERFACE_LINK_LIBRARIES ${_all_targets})
    endif()
endif()

message(STATUS "RobotpyNative: ${ROBOTPY_NATIVE_DIR}")
