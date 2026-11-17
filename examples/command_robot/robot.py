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
Minimal command-based robot built on cpy6107.TimedCommandRobot.

Run in simulation with the robotpy CLI (``robotpy sim``) or hand ``Robot`` to ``wpilib.run``.
"""

from __future__ import annotations

import commands2
from commands2 import CommandScheduler

from cpy6107 import LoopTiming, TimedCommandRobot


class Counter(commands2.Subsystem):
    """Stands in for a real subsystem; counts how often its default command runs."""

    def __init__(self) -> None:
        super().__init__()
        self.ticks = 0

    def tick(self) -> commands2.Command:
        # Commands only run while enabled unless they ignore the disabled state.
        return self.run(self._tick).ignoring_disable(True)

    def _tick(self) -> None:
        self.ticks += 1


class Robot(TimedCommandRobot):
    """Runs the command scheduler in its own slot, 5 ms after the main loop slot."""

    def __init__(self, period: float = 0.02) -> None:
        super().__init__(period)
        # Report loop overruns after 4 periods instead of the default of 1.
        self.set_watchdog_timeout(4 * period)
        # Replaces commands2.TimedCommandRobot's add_periodic(scheduler.run, ...).
        self.set_scheduler(CommandScheduler.get_instance().run)

        self.counter = Counter()
        self.counter.set_default_command(self.counter.tick())
        self.last_timing: LoopTiming | None = None

    def before_user_loop(self) -> None:
        """Hook for pre-loop work such as logging inputs; outside the watchdog window."""

    def after_user_loop(self, timing: LoopTiming) -> None:
        """Hook for post-loop work such as logging outputs."""
        self.last_timing = timing

    def robot_periodic(self) -> None:
        pass
