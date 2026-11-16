#!/usr/bin/env bash
# Run a command in the glibc 2.41 Docker image (builds the image on first use).
#
#   docker/run.sh                                  # cmake --workflow --preset linux-ci
#   docker/run.sh cmake --workflow --preset linux-clang-ci
#   docker/run.sh --wpilib 2027.0.0a6 cmake --workflow --preset linux-ci
#   CPY6107_DOCKER_ARGS="--security-opt seccomp=unconfined" docker/run.sh cmake --workflow --preset linux-tsan-ci
#   docker/run.sh --rebuild bash                   # rebuild the image, then open a shell
#
# The source tree is mounted at /work. Build output goes to build-docker/<wpilib version>/ so it
# never clashes with the host's build/ directory (CMake caches absolute paths). Files are created
# with your uid/gid.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
wpilib="2027.0.0a8"
rebuild=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --wpilib) wpilib="$2"; shift 2 ;;
        --rebuild) rebuild=1; shift ;;
        *) break ;;
    esac
done

tag="cpy6107-dev:${wpilib}"
if [[ $rebuild -eq 1 ]] || ! docker image inspect "$tag" >/dev/null 2>&1; then
    docker build -f "$root/docker/Dockerfile" --build-arg "WPILIB_VERSION=${wpilib}" -t "$tag" "$root"
fi

mkdir -p "$root/build-docker/${wpilib}"
tty_flags=()
[[ -t 0 && -t 1 ]] && tty_flags=(-it)

# Extra `docker run` flags, e.g. CPY6107_DOCKER_ARGS="--security-opt seccomp=unconfined" (the
# linux-tsan preset needs it so ThreadSanitizer can disable ASLR).
read -r -a extra_args <<<"${CPY6107_DOCKER_ARGS:-}"

exec docker run --rm "${tty_flags[@]}" "${extra_args[@]}" \
    --user "$(id -u):$(id -g)" \
    -v "$root:/work" \
    -v "$root/build-docker/${wpilib}:/work/build" \
    "$tag" "$@"
