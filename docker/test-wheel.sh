#!/usr/bin/env bash
# Install dist/*.whl into a pristine python:3.14-trixie container (no compilers, no repository
# sources on the path), then run the Python test suite against the installed wheel. This is what
# a team installing from PyPI gets, so it catches missing/unresolvable robotpy libraries.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ls "$root"/dist/*.whl >/dev/null

docker run --rm -v "$root:/work:ro" -w /tmp python:3.14-trixie bash -c '
set -euo pipefail
python -m venv /tmp/v && . /tmp/v/bin/activate
pip install -q --pre /work/dist/*.whl pytest pytest-cov
pip list 2>/dev/null | grep -i -E "^(cpy6107|wpilib) "
python -c "import cpy6107; print(cpy6107.__file__)"
cp -r /work/tests/python /tmp/t && cd /tmp/t
python -m pytest -q -p no:cacheprovider --no-cov .
'
