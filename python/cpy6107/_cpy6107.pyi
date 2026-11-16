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
"""
C++23 TimedCommandRobot with a settable loop watchdog for FRC 2027 (robotpy).
"""

from collections.abc import Callable
import enum
from typing import Final


class HookErrorPolicy(enum.Enum):
    """
    What to do when a before/after_user_loop hook or the overrun callback raises.
    """

    LOG_AND_CONTINUE = 0
    """Report the error and keep the loop running (default)."""

    STOP = 1
    """Re-raise out of start_competition()."""

class OverrunInfo:
    """One loop cycle that exceeded the watchdog timeout."""

    @property
    def loop_time(self) -> float:
        """Seconds spent in before_user_loop() plus the loop body."""

    @property
    def timeout(self) -> float:
        """Watchdog timeout in seconds."""

class LoopTiming:
    """Time spent in the two phases of one main-loop cycle (seconds)."""

    @property
    def pre_user_time(self) -> float:
        """Seconds spent in before_user_loop()."""

    @property
    def user_time(self) -> float:
        """
        Seconds spent in the mode periodic, robot_periodic and simulation_periodic methods.
        """

class TimedCommandRobot:
    """
    Periodic robot base class that owns its loop watchdog so the timeout can be set.

    Subclass it and override the usual robotpy hooks (robot_periodic, teleop_periodic, ...). Pass the class to wpilib.run(). Each period one slot runs the robot loop; a second slot, offset by `scheduler_offset`, runs the callable given to set_scheduler().
    """

    def __init__(self, period: float = 0.02, scheduler_offset: float = 0.005) -> None:
        """
        Create the robot. `period` and `scheduler_offset` are in seconds; raises ValueError if period <= 0 or scheduler_offset < 0.
        """

    DEFAULT_PERIOD: Final[float] = ...
    """Default loop period in seconds (0.02)."""

    SCHEDULER_OFFSET: Final[float] = ...
    """
    Default offset of the scheduler slot from the loop slot, in seconds (0.005).
    """

    def start_competition(self) -> None:
        """
        Run the robot loop until end_competition() is called. Releases the GIL while waiting.
        """

    def end_competition(self) -> None:
        """Stop the loop; safe to call from any thread."""

    def add_periodic(self, callback: Callable[[], None], period: float, offset: float = 0.0) -> None:
        """
        Call `callback` every `period` seconds, `offset` seconds after the common start time, on the robot thread. Call before start_competition().
        """

    def set_scheduler(self, scheduler: Callable[[], None]) -> None:
        """
        Set the command scheduler callable, run once per period at the scheduler offset. Call before start_competition().
        """

    def clear_scheduler(self) -> None:
        """Remove the scheduler callable."""

    def set_watchdog_timeout(self, timeout: float) -> None:
        """
        Set the loop overrun threshold in seconds (default: the loop period). Raises ValueError if timeout <= 0.
        """

    def get_watchdog_timeout(self) -> float:
        """Loop overrun threshold in seconds."""

    def get_period(self) -> float:
        """Loop period in seconds."""

    def add_watchdog_epoch(self, name: str) -> None:
        """Add a named epoch to the current cycle's watchdog."""

    def print_watchdog_epochs(self) -> None:
        """Print the current cycle's watchdog epochs."""

    def get_loop_start_time(self) -> float:
        """
        Monotonic time, in seconds, at the start of the current periodic callback.
        """

    def set_hook_error_policy(self, policy: HookErrorPolicy) -> None:
        """
        Choose what happens when before_user_loop, after_user_loop or the overrun callback raises. Default LOG_AND_CONTINUE. Call before start_competition().
        """

    def get_hook_error_policy(self) -> HookErrorPolicy:
        """Current hook error policy."""

    def get_hook_error_count(self) -> int:
        """Number of hook exceptions swallowed under LOG_AND_CONTINUE."""

    def set_overrun_callback(self, callback: Callable[[OverrunInfo], None] | None) -> None:
        """
        Call `callback(info: OverrunInfo)` on the robot thread after each cycle whose watchdog expired. Pass None to clear. Call before start_competition().
        """

    def before_user_loop(self) -> None:
        """Override: runs just before the loop body, outside the watchdog window."""

    def after_user_loop(self, timing: LoopTiming) -> None:
        """Override: runs just after the loop body, outside the watchdog window."""

    @staticmethod
    def is_real() -> bool:
        """True when running on a robot."""

    @staticmethod
    def is_simulation() -> bool:
        """True when running in simulation."""

    @staticmethod
    def is_enabled() -> bool:
        """True when the robot is enabled."""

    @staticmethod
    def is_disabled() -> bool:
        """True when the robot is disabled."""

    @staticmethod
    def is_autonomous() -> bool:
        """True in autonomous mode, enabled or not."""

    @staticmethod
    def is_autonomous_enabled() -> bool:
        """True when enabled in autonomous mode."""

    @staticmethod
    def is_teleop() -> bool:
        """True in teleoperated mode, enabled or not."""

    @staticmethod
    def is_teleop_enabled() -> bool:
        """True when enabled in teleoperated mode."""

    @staticmethod
    def is_utility() -> bool:
        """True in utility mode, enabled or not."""

    @staticmethod
    def is_utility_enabled() -> bool:
        """True when enabled in utility mode."""

    @staticmethod
    def get_opmode_id() -> int:
        """Id of the selected operation mode."""

    @staticmethod
    def get_opmode() -> str:
        """Name of the selected operation mode."""

    def driver_station_connected(self) -> None:
        """Override: called once when the driver station first connects."""

    def simulation_init(self) -> None:
        """Override: simulation initialization."""

    def disabled_init(self) -> None:
        """Override: entering disabled."""

    def autonomous_init(self) -> None:
        """Override: entering autonomous."""

    def teleop_init(self) -> None:
        """Override: entering teleoperated."""

    def utility_init(self) -> None:
        """Override: entering utility mode."""

    def robot_periodic(self) -> None:
        """Override: runs every cycle in all modes."""

    def simulation_periodic(self) -> None:
        """Override: runs every cycle in simulation."""

    def disabled_periodic(self) -> None:
        """Override: periodic while disabled."""

    def autonomous_periodic(self) -> None:
        """Override: periodic during autonomous."""

    def teleop_periodic(self) -> None:
        """Override: periodic during teleop."""

    def utility_periodic(self) -> None:
        """Override: periodic during utility mode."""

    def disabled_exit(self) -> None:
        """Override: leaving disabled."""

    def autonomous_exit(self) -> None:
        """Override: leaving autonomous."""

    def teleop_exit(self) -> None:
        """Override: leaving teleoperated."""

    def utility_exit(self) -> None:
        """Override: leaving utility mode."""
