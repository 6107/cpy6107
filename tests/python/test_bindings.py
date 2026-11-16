"""Tests for the cpy6107.TimedCommandRobot Python bindings."""

from __future__ import annotations

import threading
import time
from collections.abc import Callable, Iterator
from contextlib import contextmanager

import pytest

import cpy6107
from cpy6107 import LoopTiming, TimedCommandRobot

WAIT_S = 5.0


def wait_until(predicate: Callable[[], bool], timeout: float = WAIT_S) -> bool:
    deadline = time.monotonic() + timeout
    while not predicate():
        if time.monotonic() > deadline:
            return False
        time.sleep(0.001)
    return True


@contextmanager
def running(robot: TimedCommandRobot) -> Iterator[threading.Thread]:
    """Run startCompetition() on a thread and always stop it afterwards."""
    thread = threading.Thread(target=robot.startCompetition, daemon=True)
    thread.start()
    try:
        yield thread
    finally:
        robot.endCompetition()
        thread.join(WAIT_S)
        assert not thread.is_alive()


class Probe(TimedCommandRobot):
    def __init__(self, *args: float) -> None:
        super().__init__(*args)
        self.counts: dict[str, int] = {}
        self.timings: list[LoopTiming] = []
        self.starts: list[float] = []

    def _bump(self, name: str) -> None:
        self.counts[name] = self.counts.get(name, 0) + 1

    def count(self, name: str) -> int:
        return self.counts.get(name, 0)

    def beforeUserLoop(self) -> None:
        self._bump("before")

    def afterUserLoop(self, timing: LoopTiming) -> None:
        self._bump("after")
        self.timings.append(timing)

    def simulationInit(self) -> None:
        self._bump("simulationInit")

    def disabledInit(self) -> None:
        self._bump("disabledInit")

    def disabledPeriodic(self) -> None:
        self._bump("disabledPeriodic")

    def robotPeriodic(self) -> None:
        self._bump("robotPeriodic")
        self.starts.append(self.getLoopStartTime())


def test_module_exports() -> None:
    assert set(cpy6107.__all__) >= {"TimedCommandRobot", "LoopTiming"}
    assert pytest.approx(0.02) == TimedCommandRobot.DEFAULT_PERIOD
    assert pytest.approx(0.005) == TimedCommandRobot.SCHEDULER_OFFSET


def test_constructor_defaults_and_arguments() -> None:
    robot = TimedCommandRobot()
    assert robot.getPeriod() == pytest.approx(0.02)
    assert robot.getWatchdogTimeout() == pytest.approx(0.02)
    custom = TimedCommandRobot(0.05, 0.01)
    assert custom.getPeriod() == pytest.approx(0.05)


@pytest.mark.parametrize(("period", "offset"), [(0.0, 0.005), (-0.02, 0.005), (0.02, -0.001)])
def test_constructor_rejects_invalid_values(period: float, offset: float) -> None:
    with pytest.raises(ValueError):
        TimedCommandRobot(period, offset)


def test_watchdog_timeout_is_settable() -> None:
    robot = TimedCommandRobot()
    robot.setWatchdogTimeout(0.1)
    assert robot.getWatchdogTimeout() == pytest.approx(0.1)
    for bad in (0.0, -1.0):
        with pytest.raises(ValueError):
            robot.setWatchdogTimeout(bad)
    assert robot.getWatchdogTimeout() == pytest.approx(0.1)


def test_end_competition_before_start_is_safe() -> None:
    robot = TimedCommandRobot()
    robot.endCompetition()
    robot.endCompetition()
    robot.startCompetition()


def test_overridden_hooks_run_with_timing() -> None:
    robot = Probe()
    with running(robot):
        assert wait_until(lambda: robot.count("after") >= 5)
    assert robot.count("simulationInit") == 1
    assert robot.count("disabledInit") == 1
    assert robot.count("disabledPeriodic") >= 5
    assert robot.count("robotPeriodic") >= 5
    assert robot.count("before") >= robot.count("after")
    assert all(t.pre_user_time >= 0.0 and t.user_time > 0.0 for t in robot.timings)
    assert robot.starts[0] > 0.0
    assert robot.starts == sorted(robot.starts)


def test_default_hooks_are_callable_from_subclass_super() -> None:
    class Calls(TimedCommandRobot):
        done = False

        def robotPeriodic(self) -> None:
            super().robotPeriodic()
            super().disabledPeriodic()
            super().beforeUserLoop()
            super().simulationPeriodic()
            self.done = True

    robot = Calls()
    with running(robot):
        assert wait_until(lambda: robot.done)


def test_default_hooks_are_noops_when_called_directly() -> None:
    robot = TimedCommandRobot()
    for name in (
        "beforeUserLoop",
        "driverStationConnected",
        "simulationInit",
        "disabledInit",
        "autonomousInit",
        "teleopInit",
        "utilityInit",
        "robotPeriodic",
        "simulationPeriodic",
        "disabledPeriodic",
        "autonomousPeriodic",
        "teleopPeriodic",
        "utilityPeriodic",
        "disabledExit",
        "autonomousExit",
        "teleopExit",
        "utilityExit",
    ):
        getattr(robot, name)()


def test_scheduler_runs_each_period_and_can_be_cleared() -> None:
    robot = Probe(0.02, 0.01)
    runs: list[float] = []
    robot.setScheduler(lambda: runs.append(time.monotonic()))
    with running(robot):
        assert wait_until(lambda: len(runs) >= 5)

    cleared = Probe()
    calls: list[int] = []
    cleared.setScheduler(lambda: calls.append(1))
    cleared.clearScheduler()
    with running(cleared):
        assert wait_until(lambda: cleared.count("after") >= 5)
    assert calls == []


def test_add_periodic_runs_at_its_own_rate() -> None:
    robot = Probe()
    calls: list[int] = []
    robot.addPeriodic(lambda: calls.append(1), 0.01, 0.002)
    with running(robot):
        assert wait_until(lambda: len(calls) >= 8)


def test_watchdog_epoch_helpers_from_hooks() -> None:
    class Epochs(Probe):
        def robotPeriodic(self) -> None:
            super().robotPeriodic()
            self.addWatchdogEpoch("python-epoch")
            self.printWatchdogEpochs()

    robot = Epochs()
    robot.setWatchdogTimeout(0.000001)  # forces an overrun report; the loop must keep running
    with running(robot):
        assert wait_until(lambda: robot.count("after") >= 4)


def test_exception_in_hook_propagates_out_of_start_competition() -> None:
    class Boom(TimedCommandRobot):
        def robotPeriodic(self) -> None:
            raise RuntimeError("boom")

    robot = Boom()
    errors: list[BaseException] = []

    def target() -> None:
        try:
            robot.startCompetition()
        except RuntimeError as exc:
            errors.append(exc)

    thread = threading.Thread(target=target, daemon=True)
    thread.start()
    thread.join(WAIT_S)
    robot.endCompetition()
    assert not thread.is_alive()
    assert errors
    assert "boom" in str(errors[0])


def test_other_python_threads_run_during_the_loop() -> None:
    robot = Probe()
    ticks: list[int] = []

    def worker() -> None:
        for _ in range(20):
            ticks.append(1)
            time.sleep(0.002)

    with running(robot):
        thread = threading.Thread(target=worker)
        thread.start()
        thread.join(WAIT_S)
        assert len(ticks) == 20
        assert wait_until(lambda: robot.count("after") > 0)
