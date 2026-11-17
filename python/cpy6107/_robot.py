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

"""``TimedCommandRobot``: a real ``wpilib.IterativeRobotBase`` driven by the C++ timed loop.

nanobind classes cannot inherit from robotpy's pybind11 ``wpilib.IterativeRobotBase``, so this
class subclasses it in Python (``isinstance`` checks and WPILib APIs that take a robot work) and
delegates the periodic loop to a private C++ engine that calls back into this object.
"""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

import wpilib

from . import _cpy6107
from ._cpy6107 import HookErrorPolicy, LoopTiming, OverrunInfo

_HOOKS = (
    "driver_station_connected",
    "simulation_init",
    "disabled_init",
    "autonomous_init",
    "teleop_init",
    "utility_init",
    "robot_periodic",
    "simulation_periodic",
    "disabled_periodic",
    "autonomous_periodic",
    "teleop_periodic",
    "utility_periodic",
    "disabled_exit",
    "autonomous_exit",
    "teleop_exit",
    "utility_exit",
)


# wpilib 2027 a6 still exposes camelCase names; alias the snake_case ones the loop and tests use.
_COMPAT_NAMES = (
    "is_real",
    "is_simulation",
    "is_enabled",
    "is_disabled",
    "is_autonomous",
    "is_autonomous_enabled",
    "is_teleop",
    "is_teleop_enabled",
    "is_utility",
    "is_utility_enabled",
    "get_period",
    "get_opmode_id",
    "get_opmode",
)


def _camel(name: str) -> str:
    if name.startswith("get_opmode"):
        return name.replace("get_opmode", "getOpMode").replace("_id", "Id")
    head, *rest = name.split("_")
    return head + "".join(part.capitalize() for part in rest)


class _Engine(_cpy6107.TimedCommandRobot):
    """The C++ loop; every overridable hook is forwarded to the owning robot."""

    def __init__(self, owner: TimedCommandRobot, period: float, scheduler_offset: float) -> None:
        super().__init__(period, scheduler_offset)
        self._owner = owner

    def before_user_loop(self) -> None:
        self._owner.before_user_loop()

    def after_user_loop(self, timing: LoopTiming) -> None:
        self._owner.after_user_loop(timing)


def _forward(name: str) -> Callable[[_Engine], None]:
    def hook(self: _Engine) -> None:
        getattr(self._owner, name)()

    hook.__name__ = name
    return hook


for _name in _HOOKS:
    setattr(_Engine, _name, _forward(_name))


class TimedCommandRobot(wpilib.IterativeRobotBase):
    """Periodic robot base class that owns its loop watchdog so the timeout can be set.

    Subclass it and override the usual robotpy hooks (robot_periodic, teleop_periodic, ...). Pass
    the class to wpilib.run(). Each period one slot runs the robot loop; a second slot, offset by
    ``scheduler_offset``, runs the callable given to set_scheduler().
    """

    DEFAULT_PERIOD: float = _cpy6107.TimedCommandRobot.DEFAULT_PERIOD
    SCHEDULER_OFFSET: float = _cpy6107.TimedCommandRobot.SCHEDULER_OFFSET

    def __init__(
        self, period: float = DEFAULT_PERIOD, scheduler_offset: float = SCHEDULER_OFFSET
    ) -> None:
        # Build the engine first so invalid arguments raise ValueError before WPILib is touched.
        engine = _Engine(self, period, scheduler_offset)
        super().__init__(period)
        self._engine = engine

    def start_competition(self) -> None:
        """Run the robot loop until end_competition() is called. Releases the GIL while waiting."""
        self._engine.start_competition()

    def end_competition(self) -> None:
        """Stop the loop; safe to call from any thread."""
        self._engine.end_competition()

    def add_periodic(self, callback: Callable[[], Any], period: float, offset: float = 0.0) -> None:
        """Call ``callback`` every ``period`` seconds, ``offset`` seconds after the start time."""
        self._engine.add_periodic(callback, period, offset)

    def set_scheduler(self, scheduler: Callable[[], Any]) -> None:
        """Set the command scheduler callable, run once per period at the scheduler offset."""
        self._engine.set_scheduler(scheduler)

    def clear_scheduler(self) -> None:
        """Remove the scheduler callable."""
        self._engine.clear_scheduler()

    def set_watchdog_timeout(self, timeout: float) -> None:
        """Set the loop overrun threshold in seconds. Raises ValueError if timeout <= 0."""
        self._engine.set_watchdog_timeout(timeout)

    def get_watchdog_timeout(self) -> float:
        """Loop overrun threshold in seconds."""
        return self._engine.get_watchdog_timeout()

    def add_watchdog_epoch(self, name: str) -> None:
        """Add a named epoch to the current cycle's watchdog."""
        self._engine.add_watchdog_epoch(name)

    def print_watchdog_epochs(self) -> None:
        """Print the current cycle's watchdog epochs."""
        self._engine.print_watchdog_epochs()

    def get_loop_start_time(self) -> float:
        """Monotonic time, in seconds, at the start of the current periodic callback."""
        return self._engine.get_loop_start_time()

    def set_hook_error_policy(self, policy: HookErrorPolicy) -> None:
        """Choose what happens when before/after_user_loop or the overrun callback raises."""
        self._engine.set_hook_error_policy(policy)

    def get_hook_error_policy(self) -> HookErrorPolicy:
        """Current hook error policy."""
        return self._engine.get_hook_error_policy()

    def get_hook_error_count(self) -> int:
        """Number of hook exceptions swallowed under LOG_AND_CONTINUE."""
        return self._engine.get_hook_error_count()

    def set_overrun_callback(self, callback: Callable[[OverrunInfo], Any] | None) -> None:
        """Call ``callback(info)`` after each cycle whose watchdog expired; None clears it."""
        self._engine.set_overrun_callback(callback)

    def before_user_loop(self) -> None:
        """Override: runs just before the loop body, outside the watchdog window."""

    def after_user_loop(self, timing: LoopTiming) -> None:
        """Override: runs just after the loop body, outside the watchdog window."""


# Hooks are defined here (not inherited) so the engine can always call them, on any wpilib naming.
def _noop_hook(name: str) -> Callable[[TimedCommandRobot], None]:
    def hook(self: TimedCommandRobot) -> None:
        """Override in a subclass."""

    hook.__name__ = name
    return hook


for _name in _HOOKS:
    setattr(TimedCommandRobot, _name, _noop_hook(_name))

for _name in _COMPAT_NAMES:
    if not hasattr(wpilib.IterativeRobotBase, _name):
        for _base in wpilib.IterativeRobotBase.__mro__:
            if _camel(_name) in vars(_base):
                setattr(TimedCommandRobot, _name, vars(_base)[_camel(_name)])
                break
