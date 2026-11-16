# Contributing to cpy6107

Thanks for helping. cpy6107 is maintained by FRC Team 6107 (CyberJagzz) and is free for any team to
use. By contributing you agree your work is released under the project [LICENSE](LICENSE).
Please follow the [Code of Conduct](CODE_OF_CONDUCT.md).

This repository was set up using github copilot, but the intention is to use AI for dev-ops, unit
test generation, documentation, and general automation. Try no to use it to initially write your
code as we are here to learn. Do feel free to use AI to help debug and improve your code once you
start. Understanding the C/C++ language, programming a robot, and using AI to improve your productiveness
and improve code quality is what this is all about.

## Reporting a bug

1. Check the [troubleshooting table](docs/GETTING-STARTED.md#6-troubleshooting) and existing issues.
2. Open a **Bug report** at <https://github.com/6107/cpy6107/issues/new/choose>. The template asks
   for what we need: cpy6107 and WPILib versions, OS and glibc, Python version, how you installed,
   a minimal robot that shows the problem, and the full output.
3. Security problems: do **not** open a public issue; see [SECURITY.md](SECURITY.md).

## Asking for a change or a feature

Open a **Change request** issue. Describe the problem you have (not only the solution), what you
do today, and what you would like instead. Changes to the public Python API are breaking while the
version is an alpha, so we will ask what it would cost you. We are a small development group
at this time and well documented requirements for a change or feature will always be welcomed.

Since we are a small group, we may not be able to address a feature until after build season. If
you see a change or feature that you would like to tackle, let me known. The more eyes and brains
on a project really help to make it better and more accepted to a larger community.

## Sending a pull request

1. Open or find an issue first for anything larger than a typo.
2. Fork, branch, make a focused change.
3. Run the whole check; it must pass:

   ```bash
   cmake --workflow --preset linux-ci          # configure, build, lint (C++ and Python), test
   # no glibc >= 2.41 host (WPILib a8)? use Docker:
   docker/run.sh cmake --workflow --preset linux-ci
   ```

   `clang-format`, `clang-tidy`, `ruff`, `mypy` and both test suites are included. Details and the
   other presets (Clang, coverage) are in [docs/INSTALL.md](docs/INSTALL.md) and [AGENTS.md](AGENTS.md).
4. Add or update tests for what you changed, and update the docs and `CHANGELOG.md` (user-visible
   changes).
5. Open the PR; the template has the checklist. CI runs on GitHub Actions.

### Code rules (short version)

* C++23, zero warnings under `-Wall -Wextra -Wpedantic -Werror`; no `using namespace` in headers.
* `include/cpy6107/core` and `src/core` are plain C++: no OS headers, no WPILib, no allocation in
  per-cycle code. Hardware and simulation code go in `hal` and `sim`.
* Nanobind code lives only in `bindings/`; never include `<nanobind/nanobind.h>` or `<Python.h>`
  from `include/` or `src/`.
* Python names are snake_case, times are seconds.
* WPILib 2027 `wpi::` namespaces only.

The full rules are in `.github/instructions/` and `AGENTS.md`.

## Releases

Maintainers only; see [docs/RELEASING.md](docs/RELEASING.md).
