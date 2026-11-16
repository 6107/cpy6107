"""
C++23 TimedCommandRobot with a settable loop watchdog for FRC 2027 (robotpy).
"""

from collections.abc import Callable
from typing import Final


class LoopTiming:
    """Time spent in the two phases of one main-loop cycle (seconds)."""

    @property
    def pre_user_time(self) -> float:
        """Seconds spent in beforeUserLoop()."""

    @property
    def user_time(self) -> float:
        """
        Seconds spent in the mode periodic, robotPeriodic and simulationPeriodic methods.
        """

class TimedCommandRobot:
    """
    Periodic robot base class that owns its loop watchdog so the timeout can be set.

    Subclass it and override the usual robotpy hooks (robotPeriodic, teleopPeriodic, ...). Pass the class to wpilib.run(). Each period one slot runs the robot loop; a second slot, offset by `scheduler_offset`, runs the callable given to setScheduler().
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

    def startCompetition(self) -> None:
        """
        Run the robot loop until endCompetition() is called. Releases the GIL while waiting.
        """

    def endCompetition(self) -> None:
        """Stop the loop; safe to call from any thread."""

    def addPeriodic(self, callback: Callable[[], None], period: float, offset: float = 0.0) -> None:
        """
        Call `callback` every `period` seconds, `offset` seconds after the common start time, on the robot thread. Call before startCompetition().
        """

    def setScheduler(self, scheduler: Callable[[], None]) -> None:
        """
        Set the command scheduler callable, run once per period at the scheduler offset. Call before startCompetition().
        """

    def clearScheduler(self) -> None:
        """Remove the scheduler callable."""

    def setWatchdogTimeout(self, timeout: float) -> None:
        """
        Set the loop overrun threshold in seconds (default: the loop period). Raises ValueError if timeout <= 0.
        """

    def getWatchdogTimeout(self) -> float:
        """Loop overrun threshold in seconds."""

    def getPeriod(self) -> float:
        """Loop period in seconds."""

    def addWatchdogEpoch(self, name: str) -> None:
        """Add a named epoch to the current cycle's watchdog."""

    def printWatchdogEpochs(self) -> None:
        """Print the current cycle's watchdog epochs."""

    def getLoopStartTime(self) -> float:
        """
        Monotonic time, in seconds, at the start of the current periodic callback.
        """

    def beforeUserLoop(self) -> None:
        """Override: runs just before the loop body, outside the watchdog window."""

    def afterUserLoop(self, timing: LoopTiming) -> None:
        """Override: runs just after the loop body, outside the watchdog window."""

    def driverStationConnected(self) -> None:
        """Override: called once when the driver station first connects."""

    def simulationInit(self) -> None:
        """Override: simulation initialization."""

    def disabledInit(self) -> None:
        """Override: entering disabled."""

    def autonomousInit(self) -> None:
        """Override: entering autonomous."""

    def teleopInit(self) -> None:
        """Override: entering teleoperated."""

    def utilityInit(self) -> None:
        """Override: entering utility mode."""

    def robotPeriodic(self) -> None:
        """Override: runs every cycle in all modes."""

    def simulationPeriodic(self) -> None:
        """Override: runs every cycle in simulation."""

    def disabledPeriodic(self) -> None:
        """Override: periodic while disabled."""

    def autonomousPeriodic(self) -> None:
        """Override: periodic during autonomous."""

    def teleopPeriodic(self) -> None:
        """Override: periodic during teleop."""

    def utilityPeriodic(self) -> None:
        """Override: periodic during utility mode."""

    def disabledExit(self) -> None:
        """Override: leaving disabled."""

    def autonomousExit(self) -> None:
        """Override: leaving autonomous."""

    def teleopExit(self) -> None:
        """Override: leaving teleoperated."""

    def utilityExit(self) -> None:
        """Override: leaving utility mode."""
