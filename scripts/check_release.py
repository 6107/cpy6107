#!/usr/bin/env python3
"""Fail unless the git tag, the project version and the WPILib pin agree.

The version scheme is ``<WPILib version>.postN`` (for example ``2027.0.0a8.post1`` is built
against ``wpilib==2027.0.0a8``), and the tag is ``v<version>``.

    python scripts/check_release.py v2027.0.0a8.post1
"""

from __future__ import annotations

import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    tag = sys.argv[1] if len(sys.argv) > 1 else None
    data = tomllib.loads((ROOT / "pyproject.toml").read_text())
    version: str = data["project"]["version"]
    errors: list[str] = []

    match = re.fullmatch(r"(?P<wpilib>\d+\.\d+\.\d+(?:a|b|rc)?\d*)\.post\d+", version)
    if not match:
        errors.append(f"version {version!r} is not '<WPILib version>.postN'")
    else:
        pin = f"wpilib=={match['wpilib']}"
        if pin not in data["project"]["dependencies"]:
            errors.append(f"[project] dependencies must contain {pin!r}")
        if pin not in data["build-system"]["requires"]:
            errors.append(f"[build-system] requires must contain {pin!r}")
    if tag is not None and tag != f"v{version}":
        errors.append(f"tag {tag!r} does not match version {version!r} (expected v{version})")
    if f"## [{version}]" not in (ROOT / "CHANGELOG.md").read_text():
        errors.append(f"CHANGELOG.md has no '## [{version}]' section")

    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    if not errors:
        print(f"release check OK: {version}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
