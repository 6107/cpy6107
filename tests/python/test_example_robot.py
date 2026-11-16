# ruff: noqa: E402
"""The example robot in examples/command_robot runs a command-based loop in simulation."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

commands2 = pytest.importorskip("commands2")
if not hasattr(commands2.CommandScheduler, "cancel_all"):
    pytest.skip("needs the 2027 snake_case commands2 (a8)", allow_module_level=True)
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "examples" / "command_robot"))

from commands2 import CommandScheduler
from robot import Robot

from test_bindings import running, wait_until


def test_scheduler_runs_default_command() -> None:
    scheduler = CommandScheduler.get_instance()
    scheduler.cancel_all()
    robot = Robot(0.01)
    assert robot.get_watchdog_timeout() == pytest.approx(0.04)
    with running(robot):
        assert wait_until(lambda: robot.counter.ticks >= 5)
        assert wait_until(lambda: robot.last_timing is not None)
