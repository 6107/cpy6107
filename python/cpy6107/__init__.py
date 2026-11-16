# ------------------------------------------------------------------------ #
#      o-o      o                o                                         #
#     /         |                |                                         #
#    O     o  o O-o  o-o o-o     |  oo o--o o-o o-o                        #
#     \    |  | |  | |-' |   \   o | | |  |  /   /                         #
#      o-o o--O o-o  o-o o    o-o  o-o-o--O o-o o-o                        #
#             |                           |                                #
#          o--o                        o--o                                #
#                        o--o      o         o                             #
#                        |   |     |         |  o                          #
#                        O-Oo  o-o O-o  o-o -o-    o-o o-o                 #
#                        |  \  | | |  | | |  |  | |     \                  #
#                        o   o o-o o-o  o-o  o  |  o-o o-o                 #
#                                                                          #
#    Jemison High School - Huntsville Alabama                              #
# ------------------------------------------------------------------------ #

from __future__ import annotations

import os
import sys
from pathlib import Path

# On Windows, ensure bundled WPILib DLLs inside this package directory can be loaded
if sys.platform == "win32":
    _pkg_dir = Path(__file__).resolve().parent
    if _pkg_dir.exists():
        os.add_dll_directory(str(_pkg_dir))

# robotpy loads the WPILib shared libraries this extension links against; import it first so the
# loader reuses them (and so both share one HAL / NetworkTables instance).
import wpilib as _wpilib  # noqa: F401

from ._cpy6107 import HookErrorPolicy, LoopTiming, OverrunInfo, TimedCommandRobot

__all__ = [
    "HookErrorPolicy",
    "LoopTiming",
    "OverrunInfo",
    "TimedCommandRobot",
]
