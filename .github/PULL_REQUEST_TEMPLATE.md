## What and why

Closes #

## Checklist

- [ ] Only the files needed for this change are touched; unrelated cleanups are separate PRs
- [ ] **Zero warnings:** builds clean with `-Wall -Wextra -Wpedantic -Werror` (GCC and Clang)
- [ ] **Format and lint:** `clang-format`, `clang-tidy`, `ruff check`, `ruff format --check`, `mypy` pass
- [ ] **Header hygiene:** nothing in `include/` or `src/` includes `<nanobind/nanobind.h>` or `<Python.h>`
- [ ] **Stubs:** if `bindings/module.cpp` changed, the `.pyi` stub is regenerated and committed
- [ ] **Tests pass:** `cmake --workflow --preset linux-ci` (or `docker/run.sh cmake --workflow --preset linux-ci`) exits 0, with tests added or updated
- [ ] Docs and `CHANGELOG.md` updated for user-visible changes
- [ ] No new `using namespace` in headers; Python names are snake_case and times are seconds
