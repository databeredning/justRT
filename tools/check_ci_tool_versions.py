#!/usr/bin/env python3
"""Verify the pinned CI tool families and record their exact versions."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


EXPECTED_PYTHON = (3, 12)
TOOLS = (
    ("Arm GCC", ("arm-none-eabi-gcc", "--version"), r"\b13\.2\.1\b"),
    ("QEMU", ("qemu-system-arm", "--version"), r"\b8\.2(?:\.|\b)"),
    ("GDB", ("gdb-multiarch", "--version"), r"\b15(?:\.|\b)"),
)


def command_version(command: tuple[str, ...]) -> str:
    completed = subprocess.run(command, check=True, capture_output=True, text=True)
    output = completed.stdout or completed.stderr
    return output.splitlines()[0].strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    failures: list[str] = []
    python_version = ".".join(str(part) for part in sys.version_info[:3])
    lines = [f"Python: {python_version}"]
    if sys.version_info[:2] != EXPECTED_PYTHON:
        failures.append(f"Python {python_version} does not match pinned 3.12")

    for name, command, pattern in TOOLS:
        try:
            version = command_version(command)
        except (OSError, subprocess.CalledProcessError) as error:
            failures.append(f"{name} version query failed: {error}")
            continue
        lines.append(f"{name}: {version}")
        if re.search(pattern, version) is None:
            failures.append(f"{name} does not match pinned version pattern {pattern}: {version}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))

    if failures:
        print("\nCI tool version validation failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
