#!/usr/bin/env python3
"""Run the S32K312 task benchmark and emit a dynamic Markdown report."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import run_tests


INFO_RE = re.compile(
    r"JRT_BENCHMARK_INFO enabled=(\d+) tasks=(\d+) frequency=(\d+)"
)
TASK_RE = re.compile(
    r"JRT_BENCHMARK_TASK id=(\d+) name=(.*?) release=(\d+) "
    r"completion=(\d+) pending=(\d+) coalesced=(\d+) "
    r"release_max=(\d+) activation_max=(\d+) period=(\d+) "
    r"flags=(\d+) stack=(\d+) used=(\d+)"
)


def make_script(duration_ticks: int) -> str:
    lines = [
        "set pagination off",
        "set confirm off",
        "set breakpoint pending off",
        f'file "{run_tests.ELF.as_posix()}"',
        f"target remote 127.0.0.1:{run_tests.GDB_PORT}",
        "load",
        "monitor reset",
        f"set $benchmark_stop_tick = g_kernel_ticks + {duration_ticks}",
        "watch g_kernel_ticks",
        "condition $bpnum g_kernel_ticks >= $benchmark_stop_tick",
        "continue",
        'printf "JRT_BENCHMARK_INFO enabled=%u tasks=%u frequency=%u\\n", benchmark_cycle_counter_available, benchmark_task_count, benchmark_cycle_frequency_hz',
        "set $benchmark_index = 0",
        "while $benchmark_index < benchmark_task_count",
        '  printf "JRT_BENCHMARK_TASK id=%u name=%s release=%u completion=%u pending=%u coalesced=%u release_max=%u activation_max=%u period=%u flags=%u stack=%u used=%u\\n", $benchmark_index, tasks[$benchmark_index].name, benchmark_records[$benchmark_index].release_count, benchmark_records[$benchmark_index].completion_count, benchmark_records[$benchmark_index].release_count - benchmark_records[$benchmark_index].completion_count - benchmark_records[$benchmark_index].coalesced_count, benchmark_records[$benchmark_index].coalesced_count, benchmark_records[$benchmark_index].max_release_latency_cycles, benchmark_records[$benchmark_index].max_activation_cycles, benchmark_records[$benchmark_index].period_cycles, tasks[$benchmark_index].flags, tasks[$benchmark_index].stack_top - tasks[$benchmark_index].stack_bottom, tasks[$benchmark_index].high_water_words',
        "  set $benchmark_index = $benchmark_index + 1",
        "end",
        "monitor halt",
        "quit",
    ]
    return "\n".join(lines) + "\n"


def parse_report(output: str) -> tuple[dict[str, int], list[dict[str, int | str]]]:
    info_match = INFO_RE.search(output)
    if not info_match:
        raise RuntimeError("benchmark metadata was not reported\n" + output[-2000:])
    info = {
        "enabled": int(info_match.group(1)),
        "tasks": int(info_match.group(2)),
        "frequency": int(info_match.group(3)),
    }
    tasks: list[dict[str, int | str]] = []
    for match in TASK_RE.finditer(output):
        values = match.groups()
        tasks.append(
            {
                "id": int(values[0]),
                "name": values[1],
                "release": int(values[2]),
                "completion": int(values[3]),
                "pending": int(values[4]),
                "coalesced": int(values[5]),
                "release_max": int(values[6]),
                "activation_max": int(values[7]),
                "period": int(values[8]),
                "flags": int(values[9]),
                "stack": int(values[10]),
                "used": int(values[11]),
            }
        )
    if len(tasks) != info["tasks"]:
        raise RuntimeError(
            f"reported {info['tasks']} tasks but parsed {len(tasks)} task records"
        )
    return info, tasks


def format_report(info: dict[str, int], tasks: list[dict[str, int | str]]) -> str:
    frequency = info["frequency"]
    lines = [
        f"Benchmark enabled: `{info['enabled']}`",
        f"Cycle frequency: `{frequency}` Hz",
        "",
        "| Task | Releases | Runs | Pending | Coalesced | Release max (us) | Execution max (us) | Budget | Stack max | Result |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for task in tasks:
        completed = task["completion"] + task["coalesced"]
        pending = max(task["release"] - completed, 0)
        release_max_us = (
            task["release_max"] * 1_000_000 // frequency if frequency else 0
        )
        activation_max_us = (
            task["activation_max"] * 1_000_000 // frequency if frequency else 0
        )
        budget = (
            f"{task['activation_max'] * 100 / task['period']:.1f}%"
            if task["period"]
            else "N/A"
        )
        stack = (
            f"{task['used']}/{task['stack']} ({task['used'] * 100 / task['stack']:.1f}%)"
            if task["stack"]
            else "N/A"
        )
        result = (
            "CHECK"
            if task["coalesced"]
            or (task["period"] and task["activation_max"] >= task["period"])
            else "PASS"
        )
        lines.append(
            f"| {task['name']} | {task['release']} | {task['completion']} | "
            f"{pending} | {task['coalesced']} | {release_max_us} | "
            f"{activation_max_us} | {budget} | {stack} | {result} |"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--duration-ticks", type=int, default=750)
    parser.add_argument("--build", choices=("debug", "release"), default="debug")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--quiet-build", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    if args.duration_ticks <= 0:
        parser.error("--duration-ticks must be greater than zero")
    if not run_tests.executable_exists(run_tests.GDB):
        print(f"GDB not found: {run_tests.GDB}", file=sys.stderr)
        return 2
    if not run_tests.executable_exists(run_tests.JLINK):
        print(f"J-Link GDB server not found: {run_tests.JLINK}", file=sys.stderr)
        return 2

    test = run_tests.TestCase("benchmark", "g_test_benchmark.result")
    run_tests.run_build(test, args.verbose, args.quiet_build, args.build)
    server = None
    server_thread = None
    script_path: Path | None = None
    gdb = None
    lines: list[str] = []
    try:
        server, server_thread, _ = run_tests.start_jlink(args.verbose)
        script = make_script(args.duration_ticks)
        with tempfile.NamedTemporaryFile(
            "w", suffix=".gdb", prefix="justrt_benchmark_",
            dir=run_tests.ROOT / "obj", delete=False, encoding="utf-8"
        ) as script_file:
            script_file.write(script)
            script_path = Path(script_file.name)
        command = [run_tests.GDB, "--batch", "-q", "-x", str(script_path.relative_to(run_tests.ROOT))]
        gdb = subprocess.Popen(
            command, cwd=run_tests.ROOT, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1
        )
        thread = run_tests.stream_process(gdb, "[GDB] ", args.verbose, lines)
        gdb.wait(timeout=max(20.0, args.duration_ticks / 10.0))
        thread.join(timeout=1.0)
        output = "".join(lines)
        if gdb.returncode != 0:
            print(output, file=sys.stderr)
            return 1
        info, tasks = parse_report(output)
        report = format_report(info, tasks)
        print(report, end="")
        if args.output:
            args.output.write_text(report, encoding="utf-8")
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"benchmark failed: {error}", file=sys.stderr)
        if gdb is not None:
            run_tests.terminate_process(gdb)
        return 1
    finally:
        if script_path is not None:
            script_path.unlink(missing_ok=True)
        if server is not None:
            run_tests.terminate_process(server)
        if server_thread is not None:
            server_thread.join(timeout=1.0)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
