#!/usr/bin/env python3
"""Compare TimedCommandRobot::LoopBody() with upstream IterativeRobotBase::LoopFunc().

Fetches IterativeRobotBase.cpp from the allwpilib tag for a robotpy release, normalises both
function bodies (member prefixes, `watchdog_` vs `m_watchdog`, comments, whitespace) and prints a
unified diff of what is left. Expected differences are listed in docs/UPSTREAM-SYNC.md.

    python scripts/check_loopfunc.py                 # tag derived from the installed wpilib
    python scripts/check_loopfunc.py v2027.0.0-alpha-8
"""

from __future__ import annotations

import difflib
import re
import sys
import urllib.request
from importlib import metadata
from pathlib import Path

URL = (
    "https://raw.githubusercontent.com/wpilibsuite/allwpilib/{tag}/"
    "wpilibc/src/main/native/cpp/framework/IterativeRobotBase.cpp"
)
OURS = Path(__file__).resolve().parents[1] / "src/framework/timed_command_robot.cpp"


def default_tag() -> str:
    version = metadata.version("wpilib")  # e.g. 2027.0.0a8
    match = re.fullmatch(r"(\d+\.\d+\.\d+)a(\d+)(?:\.post\d+)?", version)
    if not match:
        raise SystemExit(f"cannot map wpilib {version} to an allwpilib tag; pass one explicitly")
    return f"v{match.group(1)}-alpha-{match.group(2)}"


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        depth += {"{": 1, "}": -1}.get(source[index], 0)
        if depth == 0:
            return source[brace + 1 : index]
    raise ValueError("unbalanced braces")


def normalise(text: str) -> list[str]:
    text = re.sub(r"//.*", "", text)
    text = text.replace("m_watchdog", "watchdog_").replace("m_lastMode", "last_mode_")
    text = text.replace("m_calledDsConnected", "called_ds_connected_")
    text = re.sub(r"\bwpi::internal::|\bwpi::hal::|\bwpi::nt::|\bwpi::tunables::|\bwpi::", "", text)
    text = re.sub(r"\bconst\s+", "", text)
    text = re.sub(
        r"(?:TunableRegistry::Update|SmartDashboard::UpdateValues)\(\);\s*watchdog_\.AddEpoch\(\"[^\"]*\"\);",
        "UpdateTelemetry(watchdog_);",
        text,
    )
    lines = (" ".join(line.split()) for line in text.splitlines())
    return [line for line in lines if line]


def main() -> int:
    tag = sys.argv[1] if len(sys.argv) > 1 else default_tag()
    with urllib.request.urlopen(URL.format(tag=tag), timeout=30) as response:
        upstream = response.read().decode()
    ours = body(OURS.read_text(), "void TimedCommandRobot::LoopBody()")
    theirs = body(upstream, "void IterativeRobotBase::LoopFunc()")
    diff = list(
        difflib.unified_diff(
            normalise(theirs), normalise(ours), f"upstream {tag}", "ours", lineterm=""
        )
    )
    print("\n".join(diff) if diff else f"LoopBody matches upstream LoopFunc ({tag})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
