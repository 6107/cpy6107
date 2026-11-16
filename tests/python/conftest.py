"""When CPY6107_STAGE_DIR is set (CMake's python_tests), test that build, not an installed copy."""

import os
import sys

_stage = os.environ.get("CPY6107_STAGE_DIR")
if _stage:
    # An editable install registers a meta-path finder that would shadow the staged package.
    sys.meta_path[:] = [
        f
        for f in sys.meta_path
        if "editable" not in type(f).__module__.lower()
        and "editable" not in type(f).__name__.lower()
    ]
    sys.path.insert(0, _stage)
    for _name in [m for m in sys.modules if m == "cpy6107" or m.startswith("cpy6107.")]:
        del sys.modules[_name]
