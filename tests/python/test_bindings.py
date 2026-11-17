"""Tests for the cpy6107.TimedCommandRobot Python bindings."""

from __future__ import annotations

import threading
import time
from collections.abc import Callable, Iterator
from contextlib import contextmanager

import pytest
import wpilib

import cpy6107
from cpy6107 import HookErrorPolicy, LoopTiming, OverrunInfo, TimedCommandRobot

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
    """Run start_competition() on a thread and always stop it afterwards."""
    thread = threading.Thread(target=robot.start_competition, daemon=True)
    thread.start()
    try:
        yield thread
    finally:
        robot.end_competition()
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

    def before_user_loop(self) -> None:
        self._bump("before")

    def after_user_loop(self, timing: LoopTiming) -> None:
        self._bump("after")
        self.timings.append(timing)

    def simulation_init(self) -> None:
        self._bump("simulation_init")

    def disabled_init(self) -> None:
        self._bump("disabled_init")

    def disabled_periodic(self) -> None:
        self._bump("disabled_periodic")

    def robot_periodic(self) -> None:
        self._bump("robot_periodic")
        self.starts.append(self.get_loop_start_time())


def test_module_exports() -> None:
    assert set(cpy6107.__all__) >= {"TimedCommandRobot", "LoopTiming"}
    assert pytest.approx(0.02) == TimedCommandRobot.DEFAULT_PERIOD
    assert pytest.approx(0.005) == TimedCommandRobot.SCHEDULER_OFFSET


def test_robot_is_a_wpilib_iterative_robot_base() -> None:
    robot = TimedCommandRobot()
    assert isinstance(robot, wpilib.IterativeRobotBase)
    assert isinstance(robot, wpilib.RobotBase)
    assert issubclass(Probe, wpilib.IterativeRobotBase)
    assert robot.is_simulation() and not robot.is_real()


def test_robot_can_be_passed_to_wpilib_apis() -> None:
    def takes_robot(base: wpilib.RobotBase) -> bool:
        return base.is_simulation()

    assert takes_robot(Probe())


def test_constructor_defaults_and_arguments() -> None:
    robot = TimedCommandRobot()
    assert robot.get_period() == pytest.approx(0.02)
    assert robot.get_watchdog_timeout() == pytest.approx(0.02)
    custom = TimedCommandRobot(0.05, 0.01)
    assert custom.get_period() == pytest.approx(0.05)


@pytest.mark.parametrize(("period", "offset"), [(0.0, 0.005), (-0.02, 0.005), (0.02, -0.001)])
def test_constructor_rejects_invalid_values(period: float, offset: float) -> None:
    with pytest.raises(ValueError):
        TimedCommandRobot(period, offset)


def test_watchdog_timeout_is_settable() -> None:
    robot = TimedCommandRobot()
    robot.set_watchdog_timeout(0.1)
    assert robot.get_watchdog_timeout() == pytest.approx(0.1)
    for bad in (0.0, -1.0):
        with pytest.raises(ValueError):
            robot.set_watchdog_timeout(bad)
    assert robot.get_watchdog_timeout() == pytest.approx(0.1)


def test_end_competition_before_start_is_safe() -> None:
    robot = TimedCommandRobot()
    robot.end_competition()
    robot.end_competition()
    robot.start_competition()


def test_overridden_hooks_run_with_timing() -> None:
    robot = Probe()
    with running(robot):
        assert wait_until(lambda: robot.count("after") >= 5)
    assert robot.count("simulation_init") == 1
    assert robot.count("disabled_init") == 1
    assert robot.count("disabled_periodic") >= 5
    assert robot.count("robot_periodic") >= 5
    assert robot.count("before") >= robot.count("after")
    assert all(t.pre_user_time >= 0.0 and t.user_time > 0.0 for t in robot.timings)
    assert robot.starts[0] > 0.0
    assert robot.starts == sorted(robot.starts)


def test_default_hooks_are_callable_from_subclass_super() -> None:
    class Calls(TimedCommandRobot):
        done = False

        def robot_periodic(self) -> None:
            super().robot_periodic()
            super().disabled_periodic()
            super().before_user_loop()
            super().simulation_periodic()
            self.done = True

    robot = Calls()
    with running(robot):
        assert wait_until(lambda: robot.done)


def test_default_hooks_are_noops_when_called_directly() -> None:
    robot = TimedCommandRobot()
    for name in (
        "before_user_loop",
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
    ):
        getattr(robot, name)()


def test_scheduler_runs_each_period_and_can_be_cleared() -> None:
    robot = Probe(0.02, 0.01)
    runs: list[float] = []
    robot.set_scheduler(lambda: runs.append(time.monotonic()))
    with running(robot):
        assert wait_until(lambda: len(runs) >= 5)

    cleared = Probe()
    calls: list[int] = []
    cleared.set_scheduler(lambda: calls.append(1))
    cleared.clear_scheduler()
    with running(cleared):
        assert wait_until(lambda: cleared.count("after") >= 5)
    assert calls == []


def test_add_periodic_runs_at_its_own_rate() -> None:
    robot = Probe()
    calls: list[int] = []
    robot.add_periodic(lambda: calls.append(1), 0.01, 0.002)
    with running(robot):
        assert wait_until(lambda: len(calls) >= 8)


def test_watchdog_epoch_helpers_from_hooks() -> None:
    class Epochs(Probe):
        def robot_periodic(self) -> None:
            super().robot_periodic()
            self.add_watchdog_epoch("python-epoch")
            self.print_watchdog_epochs()

    robot = Epochs()
    robot.set_watchdog_timeout(0.000001)  # forces an overrun report; the loop must keep running
    with running(robot):
        assert wait_until(lambda: robot.count("after") >= 4)


def test_exception_in_hook_propagates_out_of_start_competition() -> None:
    class Boom(TimedCommandRobot):
        def robot_periodic(self) -> None:
            raise RuntimeError("boom")

    robot = Boom()
    errors: list[BaseException] = []

    def target() -> None:
        try:
            robot.start_competition()
        except RuntimeError as exc:
            errors.append(exc)

    thread = threading.Thread(target=target, daemon=True)
    thread.start()
    thread.join(WAIT_S)
    robot.end_competition()
    assert not thread.is_alive()
    assert errors
    assert "boom" in str(errors[0])


class FailingHooks(TimedCommandRobot):
    def __init__(self) -> None:
        super().__init__()
        self.after_calls = 0

    def before_user_loop(self) -> None:
        raise RuntimeError("hook failed")

    def after_user_loop(self, timing: LoopTiming) -> None:
        self.after_calls += 1


def test_hook_exceptions_are_logged_and_loop_continues_by_default() -> None:
    robot = FailingHooks()
    assert robot.get_hook_error_policy() == HookErrorPolicy.LOG_AND_CONTINUE
    with running(robot):
        assert wait_until(lambda: robot.after_calls >= 3)
    assert robot.get_hook_error_count() >= 3


def test_stop_policy_propagates_hook_exception() -> None:
    robot = FailingHooks()
    robot.set_hook_error_policy(HookErrorPolicy.STOP)
    assert robot.get_hook_error_policy() == HookErrorPolicy.STOP
    with pytest.raises(RuntimeError, match="hook failed"):
        robot.start_competition()
    assert robot.get_hook_error_count() == 0


def test_overrun_callback_receives_info_and_can_be_cleared() -> None:
    robot = Probe()
    robot.set_watchdog_timeout(1e-6)
    infos: list[OverrunInfo] = []
    robot.set_overrun_callback(infos.append)
    with running(robot):
        assert wait_until(lambda: len(infos) >= 2)
    assert infos[0].timeout == pytest.approx(1e-6)
    assert infos[0].loop_time > 0.0
    robot.set_overrun_callback(None)


def test_overrun_callback_errors_follow_the_hook_policy() -> None:
    robot = Probe()
    robot.set_watchdog_timeout(1e-6)

    def bad(_: OverrunInfo) -> None:
        raise ValueError("bad callback")

    robot.set_overrun_callback(bad)
    with running(robot):
        assert wait_until(lambda: robot.get_hook_error_count() >= 2)


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


def test_robot_state_queries_are_available_like_wpilib_robotbase() -> None:
    robot = Probe()
    assert robot.is_simulation() and not robot.is_real()
    assert robot.is_disabled() and not robot.is_enabled()
    assert not (robot.is_autonomous() or robot.is_teleop() or robot.is_utility())
    assert not (
        robot.is_autonomous_enabled() or robot.is_teleop_enabled() or robot.is_utility_enabled()
    )
    assert isinstance(robot.get_opmode_id(), int)
    assert isinstance(robot.get_opmode(), str)
    assert TimedCommandRobot.is_simulation()
