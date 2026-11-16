# DevTools.cmake — developer workflow targets (top-level builds only, not scikit-build wheels).
#
#   dev           build everything: core, framework, C++ tests, Python extension and its stub
#   format        reformat C++ (clang-format -i) and Python (ruff format) in place
#   lint-cpp      clang-format --dry-run --Werror + clang-tidy
#   lint-py       ruff check + ruff format --check + mypy
#   lint          lint-cpp + lint-py
#   check         dev + lint, then every test (ctest: C++ and pytest against the fresh build)
#   coverage      (CPY6107_ENABLE_COVERAGE, GCC) tests + gcovr, defined in CMakeLists.txt
#
# Python tools run as `python -m <tool>` with the interpreter CMake found (the repository .venv
# if present, otherwise Python_EXECUTABLE / PATH), so they behave the same on Linux and Windows.
# C++ tools are looked up in that interpreter's venv first (pip install clang-format clang-tidy),
# then on PATH; override with -DCPY6107_CLANG_FORMAT=... / -DCPY6107_CLANG_TIDY=...

include_guard(GLOBAL)

if(NOT Python_EXECUTABLE)
    find_package(Python COMPONENTS Interpreter QUIET)
endif()

set(_hints "${CMAKE_SOURCE_DIR}/.venv/bin" "${CMAKE_SOURCE_DIR}/.venv/Scripts")
if(Python_EXECUTABLE)
    get_filename_component(_py_bin "${Python_EXECUTABLE}" DIRECTORY)
    list(PREPEND _hints "${_py_bin}")
endif()
find_program(CPY6107_CLANG_FORMAT NAMES clang-format clang-format-19 HINTS ${_hints})
find_program(CPY6107_CLANG_TIDY NAMES clang-tidy clang-tidy-19 HINTS ${_hints})

set(_src "${CMAKE_SOURCE_DIR}")
file(GLOB_RECURSE _cpp_all CONFIGURE_DEPENDS
    "${_src}/include/*.hpp" "${_src}/src/*.hpp" "${_src}/src/*.cpp"
    "${_src}/bindings/*.cpp" "${_src}/tests/cpp/*.hpp" "${_src}/tests/cpp/*.cpp" "${_src}/tests/bench/*.cpp")
file(GLOB_RECURSE _cpp_tidy CONFIGURE_DEPENDS
    "${_src}/src/*.cpp" "${_src}/bindings/*.cpp" "${_src}/tests/cpp/*.cpp" "${_src}/tests/bench/*.cpp")
set(_py_paths "${_src}/python" "${_src}/tests/python" "${_src}/scripts" "${_src}/examples")

if(NOT Python_EXECUTABLE)
    message(WARNING "DevTools: no Python interpreter found; lint-py/format Python steps unavailable")
    return()
endif()

# ---- format ------------------------------------------------------------------------------------

set(_format_cmds COMMAND "${Python_EXECUTABLE}" -m ruff format ${_py_paths})
if(CPY6107_CLANG_FORMAT)
    list(PREPEND _format_cmds COMMAND "${CPY6107_CLANG_FORMAT}" -i ${_cpp_all})
else()
    message(WARNING "DevTools: clang-format not found; 'format' skips C++")
endif()
add_custom_target(format ${_format_cmds} COMMENT "Formatting C++ and Python sources" VERBATIM)

# ---- lint --------------------------------------------------------------------------------------

add_custom_target(lint-py
    COMMAND "${Python_EXECUTABLE}" -m ruff check ${_py_paths}
    COMMAND "${Python_EXECUTABLE}" -m ruff format --check ${_py_paths}
    COMMAND "${Python_EXECUTABLE}" -m mypy
    COMMENT "Python lint: ruff check, ruff format --check, mypy"
    WORKING_DIRECTORY "${_src}"
    USES_TERMINAL VERBATIM)

add_custom_target(lint-cpp COMMENT "C++ lint: clang-format --dry-run, clang-tidy")
if(CPY6107_CLANG_FORMAT AND CPY6107_CLANG_TIDY)
    add_custom_target(lint-cpp-format
        COMMAND "${CPY6107_CLANG_FORMAT}" --dry-run --Werror ${_cpp_all}
        COMMENT "clang-format --dry-run --Werror" USES_TERMINAL VERBATIM)
    add_custom_target(lint-cpp-tidy
        COMMAND "${CPY6107_CLANG_TIDY}" -p "${CMAKE_BINARY_DIR}" --quiet ${_cpp_tidy}
        COMMENT "clang-tidy (compile_commands.json from this build)"
        WORKING_DIRECTORY "${_src}" USES_TERMINAL VERBATIM)
    add_dependencies(lint-cpp lint-cpp-format lint-cpp-tidy)
else()
    message(WARNING "DevTools: clang-format/clang-tidy not found; lint-cpp will fail. "
                    "pip install clang-format clang-tidy, or set CPY6107_CLANG_FORMAT/CPY6107_CLANG_TIDY")
    add_custom_target(lint-cpp-missing
        COMMAND "${CMAKE_COMMAND}" -E echo "clang-format/clang-tidy not found"
        COMMAND "${CMAKE_COMMAND}" -E false)
    add_dependencies(lint-cpp lint-cpp-missing)
endif()

add_custom_target(lint COMMENT "All linters")
add_dependencies(lint lint-cpp lint-py)

# ---- dev / check -------------------------------------------------------------------------------

add_custom_target(dev COMMENT "Build everything")
add_dependencies(dev cpy6107)
foreach(_t IN ITEMS cpy6107_framework cpy6107_tests cpy6107_framework_tests _cpy6107
                    _cpy6107_stub cpy6107_python_stage)
    if(TARGET ${_t})
        add_dependencies(dev ${_t})
    endif()
endforeach()

# Linters read compile_commands.json and the generated sources, so build first.
add_custom_target(check
    COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -C $<CONFIG>
    COMMENT "Running all tests (build + lint already succeeded)"
    USES_TERMINAL VERBATIM)
add_dependencies(check dev lint)
add_dependencies(lint-cpp dev)
