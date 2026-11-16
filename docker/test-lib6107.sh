#!/usr/bin/env bash
# Run lib_6107's test suite against the cpy6107 wheel in dist/, on WPILib a8 (glibc 2.41 image).
#
#   docker/test-lib6107.sh [--lib ../lib_6107] [--overlay FILE=DEST] [-- pytest args]
#
# --overlay copies FILE over DEST (a path relative to the lib_6107 root) in a scratch copy, which
# is how docs/LIB_6107_INTEGRATION.md's proposed LoggedRobot is tried without editing lib_6107.
# Build the wheel first: docker/run.sh docker/build-dist.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
lib="$root/../lib_6107"
overlays=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --lib) lib="$(cd "$2" && pwd)"; shift 2 ;;
        --overlay) overlays+=("$2"); shift 2 ;;
        --) shift; break ;;
        *) break ;;
    esac
done
lib="$(cd "$lib" && pwd)"
shopt -s nullglob
wheels=("$root"/dist/cpy6107-*.whl)
[[ ${#wheels[@]} -gt 0 ]] || { echo "no wheel in dist/; run docker/run.sh docker/build-dist.sh" >&2; exit 1; }

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
tar -C "$lib" --exclude=.venv --exclude=.git --exclude=htmlcov --exclude=htmlReport --exclude=dist \
    --exclude=__pycache__ --exclude=.idea -cf - . | tar -C "$scratch" -xf -
for o in "${overlays[@]}"; do
    cp "${o%%=*}" "$scratch/${o#*=}"
done

tag="cpy6107-dev:2027.0.0a8"
docker image inspect "$tag" >/dev/null 2>&1 || "$root/docker/run.sh" true
docker run --rm --user "$(id -u):$(id -g)" -e PYTHONDONTWRITEBYTECODE=1 -v "$scratch:/lib_6107" -v "$root/dist:/dist:ro" -w /tmp "$tag" bash -c '
set -e
python -m venv --system-site-packages /tmp/v && . /tmp/v/bin/activate
pip install -q --pre /dist/cpy6107-*.whl /lib_6107 2>&1 | grep -v notice || true
cd /lib_6107 && python -m pytest -p no:cacheprovider -q -o faulthandler_timeout=60 "$@"' bash "$@"
