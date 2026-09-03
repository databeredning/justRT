#!/usr/bin/env python3
"""Run the S32K312 justRT benchmark from flash and report results."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

try:
    from . import run_tests
except ImportError:
    import run_tests


ROOT = Path(__file__).resolve().parent.parent
ELF = ROOT / "bin" / "justrt.elf"
FLASH_IMAGE = ROOT / "bin" / "justrt.hex"
DEFAULT_GDB = "C:/devtools/gcc/gcc-10.2-arm32-eabi/bin/arm-none-eabi-gdb.exe"
DEFAULT_JLINK = "C:/Program Files/SEGGER/JLink/JLinkGDBServerCL.exe"
GDB = os.environ.get("JRT_BENCHMARK_GDB", DEFAULT_GDB)
JLINK = os.environ.get("JRT_BENCHMARK_JLINK_SERVER", DEFAULT_JLINK)
GDB_PORT = int(os.environ.get("JRT_BENCHMARK_GDB_PORT", "2331"))
TASK_RE = re.compile(
    r"JRT_BENCHMARK_TASK id=(\d+) name=(.*?) release=(\d+) "
    r"completion=(\d+) pending=(\d+) coalesced=(\d+) "
    r"release_max=(\d+) activation_max=(\d+) period=(\d+) "
    r"flags=(\d+) stack=(\d+) used=(\d+)"
)
INFO_RE = re.compile(
    r"JRT_BENCHMARK_INFO enabled=(\d+) tasks=(\d+) frequency=(\d+)"
)
STATE_RE = re.compile(
    r"JRT_BENCHMARK_STATE kernel_invariant=(\d+) fault=(\d+) fatal=(\d+) fatal_reason=(\d+) stack_fault=(\d+)"
)


def stop_process(process: subprocess.Popen[str] | None) -> None:
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=2.0)


def make_gdb_script(flash: bool) -> str:
    lines = [
        f'file "{ELF.as_posix()}"',
        "set confirm off",
        f"target remote 127.0.0.1:{GDB_PORT}",
    ]
    if flash:
        lines.extend([
            'printf "justRT: loading benchmark image into flash\\n"',
            "monitor reset",
            f'load "{FLASH_IMAGE.as_posix()}"',
        ])
    lines.extend([
        "tbreak JRT_KernelInit",
        'printf "justRT: benchmark initialization breakpoint installed\\n"',
        "monitor reset",
        'printf "justRT: target reset; running to native benchmark initialization\\n"',
        "continue",
        "finish",
        'printf "JRT_BENCHMARK_INFO enabled=%u tasks=%u frequency=%u\\n", benchmark_cycle_counter_available, benchmark_task_count, benchmark_cycle_frequency_hz',
        'printf "justRT: running native benchmark\\n"',
        "continue&",
        'echo JRT_BENCH_RUNNING\\n',
    ])
    return "\n".join(lines) + "\n"


def make_report_commands() -> str:
    lines = [
        'printf "JRT_BENCHMARK_STATE kernel_invariant=%u fault=%u fatal=%u fatal_reason=%u stack_fault=%u\\n", g_kernel_invariant_active, g_fault_active, g_fatal_active, g_fatal_reason, g_stack_fault',
        "set $benchmark_index = 0",
        "while $benchmark_index < benchmark_task_count",
        "  set $completed = benchmark_records[$benchmark_index].completion_count + benchmark_records[$benchmark_index].coalesced_count",
        "  set $pending = benchmark_records[$benchmark_index].release_count >= $completed ? benchmark_records[$benchmark_index].release_count - $completed : 0",
        '  printf "JRT_BENCHMARK_TASK id=%u name=%s release=%u completion=%u pending=%u coalesced=%u release_max=%u activation_max=%u period=%u flags=%u stack=%u used=%u\\n", $benchmark_index, tasks[$benchmark_index].name, benchmark_records[$benchmark_index].release_count, benchmark_records[$benchmark_index].completion_count, $pending, benchmark_records[$benchmark_index].coalesced_count, benchmark_records[$benchmark_index].max_release_latency_cycles, benchmark_records[$benchmark_index].max_activation_cycles, benchmark_records[$benchmark_index].period_cycles, tasks[$benchmark_index].flags, tasks[$benchmark_index].stack_top - tasks[$benchmark_index].stack_bottom, tasks[$benchmark_index].high_water_words',
        "  set $benchmark_index = $benchmark_index + 1",
        "end",
    ]
    lines.extend(("monitor halt", "quit"))
    return "\n".join(lines) + "\n"


def format_report(info: dict[str, int], tasks: list[dict[str, int | str]]) -> str:
    frequency = info["frequency"]
    lines = [
        "═" * 130,
        f"  Benchmark Enabled: {info['enabled']}  │  Cycle Frequency: {frequency:,} Hz",
        "═" * 130,
        "",
    ]
    
    # Summary statistics
    total_releases = sum(t["release"] for t in tasks)
    total_completions = sum(t["completion"] for t in tasks)
    total_coalesced = sum(t["coalesced"] for t in tasks)
    max_stack_pct = max(
        (t["used"] * 100 / t["stack"] if t["stack"] else 0)
        for t in tasks
    )
    max_exec_us = max(
        (t["activation_max"] * 1_000_000 // frequency if frequency else 0)
        for t in tasks
    )
    
    lines.extend([
        "Summary",
        "─" * 130,
        f"  Total Releases: {total_releases:,}  │  Total Completions: {total_completions:,}  │  Coalesced: {total_coalesced:,}",
        f"  Peak Stack Usage: {max_stack_pct:.1f}%  │  Max Execution: {max_exec_us} µs",
        "",
        "Task Details",
        "─" * 130,
    ])
    
    # Task table header
    lines.append(
        f"{'Task':<22} | {'Releases':>10} | {'Complete':>10} | {'Pending':>8} | {'Coalesce':>8} | "
        f"{'Latency':>10} | {'Exec Max':>10} | {'Budget':>8} | {'Stack':>15} | {'Status':>10}"
    )
    lines.append("─" * 130)
    
    # Task rows
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
            else "—"
        )
        stack = (
            f"{task['used']}/{task['stack']} ({task['used'] * 100 / task['stack']:.1f}%)"
            if task["stack"]
            else "—"
        )
        result = (
            "⚠️  CHECK"
            if task["coalesced"]
            or (task["period"] and task["activation_max"] >= task["period"])
            else "✓  PASS"
        )
        
        lines.append(
            f"{str(task['name']):<22} | {task['release']:>10,} | {task['completion']:>10,} | "
            f"{pending:>8,} | {task['coalesced']:>8,} | {release_max_us:>8} µs | "
            f"{activation_max_us:>8} µs | {budget:>8} | {stack:>15} | {result:>10}"
        )
    
    lines.extend([
        "═" * 130,
        "",
    ])
    
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=float, default=20.0, help="uninterrupted target runtime in seconds (default: 20)")
    parser.add_argument("--flash", action="store_true", help="program the justRT HEX image before running")
    parser.add_argument("--verbose", action="store_true", help="show J-Link and complete GDB output")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.runtime <= 0.0:
        raise ValueError("--runtime must be greater than zero")
    for executable, label in ((GDB, "GDB"), (JLINK, "J-Link GDB server")):
        if not Path(executable).is_file():
            raise FileNotFoundError(f"{label} not found: {executable}")
    if not ELF.is_file():
        raise FileNotFoundError(f"justRT ELF not found: {ELF}")
    if args.flash and not FLASH_IMAGE.is_file():
        raise FileNotFoundError(f"justRT flash image not found: {FLASH_IMAGE}")

    server = None
    script_path = None
    try:
        command = [JLINK, "-select", "USB", "-device", "S32K312", "-if", "JTAG", "-speed", "auto", "-port", str(GDB_PORT)]
        if not args.verbose:
            command.append("-silent")
        server = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        time.sleep(1.0)
        if server.poll() is not None:
            output = server.stdout.read() if server.stdout else ""
            raise RuntimeError(f"J-Link exited early with code {server.returncode}\n{output}")
        script_dir = ROOT / "obj"
        script_dir.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile("w", suffix=".gdb", prefix="justrt_benchmark_", dir=script_dir, delete=False, encoding="utf-8") as script:
            script.write(make_gdb_script(args.flash))
            script_path = Path(script.name)
        print(f"Using symbols from {ELF}")
        if args.flash:
            print(f"Loading and running the justRT benchmark for {args.runtime:g} seconds...")
        else:
            print(f"Resetting and running the image already in flash for {args.runtime:g} seconds...")
        gdb_script_path = script_path.relative_to(ROOT).as_posix()
        gdb = subprocess.Popen([GDB, "-q", "-x", gdb_script_path], cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        time.sleep(args.runtime)
        if gdb.stdin is None:
            raise RuntimeError("GDB stdin is unavailable")
        gdb.stdin.write("interrupt\n")
        gdb.stdin.flush()
        time.sleep(1.0)
        gdb.stdin.write(make_report_commands())
        gdb.stdin.flush()
        output, _ = gdb.communicate(timeout=10.0)
        completed = gdb
        if args.verbose:
            print(output)
        if completed.returncode != 0:
            raise RuntimeError(f"GDB exited with code {completed.returncode}\n{output[-4000:]}")
        info_match = INFO_RE.search(output)
        state_match = STATE_RE.search(output)
        rows = list(TASK_RE.finditer(output))
        if info_match is None:
            raise RuntimeError(f"Target did not reach native benchmark initialization\n{output[-4000:]}")
        info = {
            "enabled": int(info_match.group(1)),
            "tasks": int(info_match.group(2)),
            "frequency": int(info_match.group(3)),
        }
        if info["enabled"] == 0 or info["tasks"] == 0 or info["frequency"] == 0:
            raise RuntimeError(f"Invalid benchmark state after initialization: enabled={info['enabled']}, task_count={info['tasks']}, core_clock_hz={info['frequency']}")
        if state_match is None or len(rows) != info["tasks"]:
            raise RuntimeError(f"Could not read complete benchmark data\n{output[-4000:]}")
        kernel_invariant, fault, fatal, fatal_reason, stack_fault = map(int, state_match.groups())
        if kernel_invariant != 0 or fault != 0 or fatal != 0 or stack_fault != 0:
            raise RuntimeError(f"Target faulted during benchmark: kernel_invariant={kernel_invariant}, fault={fault}, fatal={fatal}, fatal_reason={fatal_reason}, stack_fault={stack_fault}")
        tasks: list[dict[str, int | str]] = []
        for match in rows:
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
        report = format_report(info, tasks)
        print(report, end="")
        return 0
    finally:
        stop_process(server)
        if script_path is not None:
            script_path.unlink(missing_ok=True)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (FileNotFoundError, RuntimeError, TimeoutError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
