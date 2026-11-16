#!/usr/bin/env bash
# Build the sdist and the Linux wheel inside the glibc 2.41 image. Run via docker/run.sh:
#
#   docker/run.sh docker/build-dist.sh        # writes dist/*.tar.gz and dist/*.whl
#
# The wheel links the robotpy-native libraries but must not bundle them (they have to be the
# same copies robotpy itself loads, so that HAL and NetworkTables are shared), hence --exclude.
# Their glibc requirement (wpihal a8: 2.41) is what makes the platform tag manylinux_2_41.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

arch="$(uname -m)"
raw="$(mktemp -d)"
trap 'rm -rf "$raw"' EXIT

rm -rf dist && mkdir dist
python -m build --sdist --outdir dist .              # isolated: installs build-system.requires
python -m build --wheel --outdir "$raw" .

excludes=()
for lib in wpilibc wpiHal MrcLib wpimath wpiutil ntcore wpinet datalog tunables telemetry; do
    excludes+=(--exclude "lib${lib}.so")
done
auditwheel show "$raw"/*.whl || true
auditwheel repair --plat "manylinux_2_41_${arch}" "${excludes[@]}" -w dist "$raw"/*.whl
ls -l dist
