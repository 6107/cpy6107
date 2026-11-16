# FetchWPILib.cmake
# Downloads WPILib C++ headers and platform-specific native libraries from frcmaven.wpi.edu
# and exposes them as clean CMake IMPORTED targets:
#   - wpilib::wpiutil
#   - wpilib::wpimath
#   - wpilib::ntcore

include_guard(GLOBAL)
include(FetchContent)

set(WPILIB_VERSION "2026.2.1" CACHE STRING "WPILib Maven release version")
set(WPILIB_MAVEN_BASE "https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first" CACHE STRING "WPILib Maven base URL")

# Determine WPILib platform classifier
if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(WPILIB_PLATFORM "windowsarm64")
    else()
        set(WPILIB_PLATFORM "windowsx86-64")
    endif()
elseif(APPLE)
    set(WPILIB_PLATFORM "osxuniversal")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(WPILIB_PLATFORM "linuxarm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7")
        set(WPILIB_PLATFORM "linuxarm32")
    else()
        set(WPILIB_PLATFORM "linuxx86-64")
    endif()
else()
    message(FATAL_ERROR "Unsupported target platform for WPILib: ${CMAKE_SYSTEM_NAME}")
endif()

message(STATUS "Configuring WPILib ${WPILIB_VERSION} for platform: ${WPILIB_PLATFORM}")

# Helper function to fetch headers + binaries and create an IMPORTED target
function(add_wpilib_maven_library GROUP_NAME ARTIFACT_NAME TARGET_ALIAS)
    set(HEADERS_CONTENT_NAME "wpilib_${ARTIFACT_NAME}_headers")
    set(BINARIES_CONTENT_NAME "wpilib_${ARTIFACT_NAME}_binaries")

    set(HEADERS_URL "${WPILIB_MAVEN_BASE}/${GROUP_NAME}/${ARTIFACT_NAME}-cpp/${WPILIB_VERSION}/${ARTIFACT_NAME}-cpp-${WPILIB_VERSION}-headers.zip")
    set(BINARIES_URL "${WPILIB_MAVEN_BASE}/${GROUP_NAME}/${ARTIFACT_NAME}-cpp/${WPILIB_VERSION}/${ARTIFACT_NAME}-cpp-${WPILIB_VERSION}-${WPILIB_PLATFORM}.zip")

    FetchContent_Declare(
        ${HEADERS_CONTENT_NAME}
        URL "${HEADERS_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(
        ${BINARIES_CONTENT_NAME}
        URL "${BINARIES_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(${HEADERS_CONTENT_NAME} ${BINARIES_CONTENT_NAME})

    set(HEADER_DIR "${${HEADERS_CONTENT_NAME}_SOURCE_DIR}")
    set(BINARY_ROOT "${${BINARIES_CONTENT_NAME}_SOURCE_DIR}")

    # Locate shared/static library paths inside extracted WPILib archive
    file(GLOB_RECURSE FOUND_SHARED_LIBS
        "${BINARY_ROOT}/**/shared/*${ARTIFACT_NAME}*"
    )
    file(GLOB_RECURSE FOUND_IMPLIBS
        "${BINARY_ROOT}/**/shared/*.lib"
    )

    if(NOT FOUND_SHARED_LIBS)
        message(FATAL_ERROR "Could not locate library for ${ARTIFACT_NAME} in ${BINARY_ROOT}")
    endif()

    list(GET FOUND_SHARED_LIBS 0 MAIN_LIB_PATH)

    add_library(${TARGET_ALIAS} SHARED IMPORTED GLOBAL)
    set_target_properties(${TARGET_ALIAS} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${HEADER_DIR}"
        IMPORTED_LOCATION "${MAIN_LIB_PATH}"
    )

    if(WIN32 AND FOUND_IMPLIBS)
        list(GET FOUND_IMPLIBS 0 IMPLIB_PATH)
        set_target_properties(${TARGET_ALIAS} PROPERTIES
            IMPORTED_IMPLIB "${IMPLIB_PATH}"
        )
    endif()

    # Collect runtime shared libraries for bundling/testing
    set_property(GLOBAL APPEND PROPERTY WPILIB_RUNTIME_LIBS "${MAIN_LIB_PATH}")
endfunction()

# Fetch core WPILib C++ libraries
add_wpilib_maven_library("wpiutil" "wpiutil" "wpilib::wpiutil")
add_wpilib_maven_library("wpimath" "wpimath" "wpilib::wpimath")
add_wpilib_maven_library("ntcore"  "ntcore"  "wpilib::ntcore")

# Wire internal WPILib dependency graph
target_link_libraries(wpilib::wpimath INTERFACE wpilib::wpiutil)
target_link_libraries(wpilib::ntcore  INTERFACE wpilib::wpiutil)
