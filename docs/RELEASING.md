# Releasing cpy6107

## 1. Decision: what is published, and why

| Question | Decision |
|---|---|
| Where | PyPI project `cpy6107` (name unclaimed as of 2026-11; it is only reserved by the first upload), with TestPyPI as the rehearsal. |
| What | One **sdist** and one **Linux x86_64 wheel** per WPILib release. Other platforms follow `docs/PLAN-2.md` steps 7 and 8f. |
| Version | `<WPILib version>.postN`, e.g. `2027.0.0a8.post1` for `wpilib==2027.0.0a8`. Same idea as robotpy's own `2027.0.0a6.post4`. `N` counts our releases for that WPILib build. |
| WPILib pin | Exact: `wpilib==2027.0.0a8` in `[project] dependencies` **and** in `[build-system] requires`. The extension is compiled against that release's C++ headers and links its native libraries, so any other release is an ABI mismatch. |
| Platform tag | `manylinux_2_39_x86_64` + `manylinux_2_41_x86_64`. The extension itself only needs glibc 2.39, but `robotpy-native-wpihal` a8 requires 2.41, so the wheel is only installable where a8 is. |
| Tooling | **Not cibuildwheel.** There is no `manylinux_2_41` build image yet; we build in the repository's own Debian trixie image (`docker/Dockerfile`, glibc 2.41), then `auditwheel repair --exclude <robotpy libs>`. |
| Bundling | The robotpy libraries are **excluded** from the wheel. They must be the very copies robotpy loads, so that the HAL and NetworkTables are shared; the wheel finds them through `$ORIGIN/../native/<pkg>/lib` RPATH entries. |
| Publishing | Tag `vX` → `.github/workflows/release.yml` → TestPyPI → PyPI with trusted publishing (OIDC). No tokens in the repository. A human pushes the tag. |

Consequences: a team on Ubuntu 24.04 (glibc 2.39) cannot use a8 at all, wheel or not. They need
glibc >= 2.41 (Debian 13, Ubuntu 25.04+) or a container; see `docs/INSTALL.md`.

An sdist install (`pip install cpy6107` where no wheel matches) builds from source and needs g++ 14+,
CMake >= 3.28 and Ninja; the WPILib headers arrive through `build-system.requires`.

## 2. Build and check locally (no publishing)

Needs only Docker.

```bash
python3 scripts/check_release.py                 # version / WPILib pin / CHANGELOG consistent
docker/run.sh docker/build-dist.sh               # dist/*.tar.gz and dist/*.whl
docker/test-wheel.sh                             # install the wheel in a pristine container, run pytest
```

`build-dist.sh` builds the sdist and the wheel with isolated builds (this also proves that
`build-system.requires` is sufficient), runs `auditwheel show` and `auditwheel repair`.
`test-wheel.sh` installs the wheel into a clean `python:3.14-trixie` container that has no compiler
and no repository sources on the path, which is what a team installing from PyPI gets.

To inspect what shipped: `unzip -l dist/*.whl` and `tar tzf dist/*.tar.gz`. The wheel must contain
`cpy6107/__init__.py`, `_cpy6107*.so`, `_cpy6107.pyi`, `py.typed` and no `libwpi*.so`.

## 3. One-time PyPI setup (maintainer)

1. Create the PyPI and TestPyPI projects by adding a **pending trusted publisher** on each
   (Account → Publishing): owner `6107`, repository `cpy6107`, workflow `release.yml`, environment
   `pypi` (PyPI) / `testpypi` (TestPyPI).
2. In GitHub → Settings → Environments create `testpypi` and `pypi`; add required reviewers to
   `pypi` so a second person approves every real release.
3. The first successful upload claims the name. Do a TestPyPI rehearsal first (section 4, step 5).

## 4. Cutting a release

1. Pick the version: new WPILib release → `<wpilib>.post1`; fix for the same WPILib → increment `N`.
2. Edit `pyproject.toml`: `version`, and the `wpilib==` pins in **both** `dependencies` and
   `build-system.requires` (a WPILib bump also needs the compat work in `docs/PLAN-2.md` step 8:
   run `python scripts/check_loopfunc.py` and the full Docker workflow for that release).
3. Edit `CHANGELOG.md`: rename `Unreleased` to the date, add a section for the next version.
4. Verify:
   ```bash
   python3 scripts/check_release.py
   docker/run.sh cmake --workflow --preset linux-ci
   docker/run.sh cmake --workflow --preset linux-clang-ci
   docker/run.sh docker/build-dist.sh && docker/test-wheel.sh
   ```
5. Rehearse: run the **Release** workflow manually (Actions → Release → Run workflow →
   `testpypi`), then in a clean glibc >= 2.41 environment:
   ```bash
   pip install --pre --index-url https://test.pypi.org/simple/ --extra-index-url https://pypi.org/simple/ cpy6107==<version>
   ```
6. Commit, then tag and push: `git tag v<version> && git push origin v<version>`. The tag must equal
   `v<version>` (the workflow runs `scripts/check_release.py` and fails otherwise).
7. Approve the `pypi` environment in GitHub when the workflow pauses.
8. Check `pip install cpy6107==<version>` in a clean environment, then create a GitHub Release from
   the tag with the changelog section.

## 5. Rollback

* **Never** re-upload a file with the same version: PyPI forbids it.
* A broken release: **yank** it (PyPI → project → Manage → the release → Yank, with a reason).
  Installs that pin it exactly still work; everything else skips it.
* Then fix forward: increment `N` (`...post2`) and release again.
* Only delete a release if it leaked something sensitive; deletion burns the version number.

## 6. Alternatives

If PyPI is not wanted, attach `dist/*` to GitHub Releases or upload to a private index
(Artifactory); `docker/build-dist.sh` is unchanged and teams use
`pip install --pre --find-links <dir or URL> cpy6107`.

## 7. Not yet covered

* Windows (`win_amd64`, delvewheel) and macOS (`universal2`, delocate) wheels: the WPILib wheels
  exist but the build has not been verified there (PLAN-2 steps 7 and 8f).
* Linux aarch64 (SystemCore): needs an ARM64 build host or qemu (PLAN-2 step 7).
* Python 3.14t (free-threaded) wheels: PLAN-2 step 10.
* `release.yml` has **never been run**.
