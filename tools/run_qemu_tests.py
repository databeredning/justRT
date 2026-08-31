#!/usr/bin/env python3
"""Automated justRT regression runner for QEMU and GDB."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TARGET = "qemu-mps2-an385"
ELF = ROOT / "bin" / TARGET / "justrt.elf"

DEFAULT_GDB = "C:/devtools/gcc/gcc-10.2-arm32-eabi/bin/arm-none-eabi-gdb.exe"
DEFAULT_QEMU = "C:/devtools/qemu/qemu-system-arm.exe"

MAKE = os.environ.get("JUSTRT_MAKE")
if not MAKE:
    MAKE = shutil.which("make") or shutil.which("mingw32-make") or "make"
GDB = os.environ.get("JUSTRT_GDB", DEFAULT_GDB)
QEMU = os.environ.get("JUSTRT_QEMU", DEFAULT_QEMU)
TEST_TIMEOUT = float(os.environ.get("JUSTRT_TEST_TIMEOUT", "20"))


@dataclass(frozen=True)
class TestCase:
    build_name: str
    result_expr: str
    diagnostic_exprs: tuple[str, ...] = ()


TESTS = (
    TestCase("boot", "g_test_boot_and_privilege", ("g_test_boot_argument",)),
    TestCase(
        "sync",
        "g_test_synchronization.result",
        (
            "g_test_synchronization.semaphore_received",
            "g_test_synchronization.queue_received",
            "g_test_synchronization.event_received",
            "g_test_synchronization.notification_received",
            "g_test_synchronization.error_code",
        ),
    ),
    TestCase(
        "mutex",
        "g_test_mutex.result",
        (
            "g_test_mutex.recursive_lock_first",
            "g_test_mutex.recursive_lock_second",
            "g_test_mutex.recursive_unlock_first",
            "g_test_mutex.recursive_unlock_second",
            "g_test_mutex.non_owner_unlock",
            "g_test_mutex.owner_priority_full_chain",
            "g_test_mutex.bridge_blocked",
            "g_test_mutex.high_blocked",
            "g_test_mutex.bridge_acquired_first",
            "g_test_mutex.high_acquired_second",
            "g_test_mutex.error_code",
        ),
    ),
    TestCase(
        "race",
        "g_test_race.result",
        (
            "g_test_race.signals",
            "g_test_race.takes",
            "g_test_race.timeouts",
            "g_test_race.lost_wakeups",
            "g_test_race.queue_signals",
            "g_test_race.queue_receives",
            "g_test_race.queue_timeouts",
            "g_test_race.queue_lost_wakeups",
            "g_test_race.queue_drain_signals",
            "g_test_race.queue_drains",
            "g_test_race.queue_sends",
            "g_test_race.queue_send_timeouts",
            "g_test_race.queue_send_lost_wakeups",
            "g_test_race.mutex_unlocks",
            "g_test_race.mutex_acquisitions",
            "g_test_race.mutex_timeouts",
            "g_test_race.mutex_post_timeout_acquisitions",
            "g_test_race.timer_stops_before_expiry",
            "g_test_race.timer_stops_after_expiry",
            "g_test_race.timer_restart_expirations",
            "g_test_race.timer_start_expirations",
            "g_test_race.timer_callbacks",
            "g_test_race.wrap_start_tick",
            "g_test_race.wrap_end_tick",
            "g_test_race.wrap_elapsed_ticks",
            "g_test_race.wrap_timeouts",
            "g_test_race.error_code",
        ),
    ),
)

KERNEL_DIAGNOSTICS = (
    "g_kernel_invariant_active",
    "g_kernel_invariant_code",
    "g_kernel_invariant_task",
    "g_kernel_invariant_object",
    "g_kernel_invariant_aux",
    "g_kernel_invariant_tick",
    "g_fault_active",
    "g_stack_fault",
    "g_stack_fault_task",
    "g_stack_fault_sp",
    "g_context_switches",
    "g_kernel_ticks",
)

RESULT_RE = re.compile(
    r"JUSTRT_RESULT state=(\d+) runs=(\d+) pass=(\d+) fail=(\d+) done=(\d+)"
)
DIAG_RE = re.compile(r"JUSTRT_DIAG ([^=]+)=(\d+)")


def executable_exists(command: str) -> bool:
    path = Path(command)
    if path.is_absolute() or path.parent != Path("."):
        return path.exists()
    return shutil.which(command) is not None


def display_command(command: list[object]) -> str:
    return " ".join('"' + str(value).replace('"', r'\"') + '"'
                    for value in command)


def stream_process(
    process: subprocess.Popen[str], prefix: str, verbose: bool, lines: list[str]
) -> threading.Thread:
    def worker() -> None:
        if process.stdout is None:
            return
        for line in process.stdout:
            lines.append(line)
            if verbose:
                print(prefix + line, end="", flush=True)

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()
    return thread


def terminate_process(process: subprocess.Popen[str] | None) -> None:
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=1.0)


def allocate_gdb_port() -> int:
    configured = os.environ.get("JUSTRT_GDB_PORT")
    if configured:
        return int(configured)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def run_build(test: TestCase, verbose: bool, quiet_build: bool) -> None:
    command = [MAKE, "-B"]
    if quiet_build:
        command.append("-s")
    command.extend((f"TARGET={TARGET}", f"TEST={test.build_name}"))
    print(f"[BUILD] {test.build_name}", flush=True)
    if verbose:
        print(f"[BUILD CMD] {display_command(command)}", flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def make_gdb_script(test: TestCase, port: int) -> str:
    result = test.result_expr
    lines = [
        "set pagination off",
        "set confirm off",
        "set breakpoint pending off",
        "set remotetimeout 5",
        f'file "{ELF.as_posix()}"',
        f"target remote 127.0.0.1:{port}",
        f"watch {result}.done",
        f"condition $bpnum {result}.done != 0",
        "watch g_kernel_invariant_active",
        "condition $bpnum g_kernel_invariant_active != 0",
        "watch g_fault_active",
        "condition $bpnum g_fault_active != 0",
        "watch g_stack_fault",
        "condition $bpnum g_stack_fault != 0",
        "continue",
        f"if {result}.done == 0",
        '  printf "JUSTRT_UNEXPECTED_STOP pc=%p\\n", $pc',
        "  x/i $pc",
        "  info registers pc lr sp xpsr",
        "  bt",
        "else",
        f'  printf "JUSTRT_RESULT state=%u runs=%u pass=%u fail=%u done=%u\\n", {result}.state, {result}.runs, {result}.pass, {result}.fail, {result}.done',
    ]
    for expression in test.diagnostic_exprs + KERNEL_DIAGNOSTICS:
        lines.append(
            f'  printf "JUSTRT_DIAG {expression}=%u\\n", {expression}'
        )
    lines.extend(("end", "disconnect", "quit"))
    return "\n".join(lines) + "\n"


def run_target(test: TestCase, verbose: bool, timeout: float) -> tuple[bool, str, float]:
    port = allocate_gdb_port()
    qemu_command = [
        QEMU,
        "-M", "mps2-an385",
        "-cpu", "cortex-m3",
        "-kernel", str(ELF),
        "-nographic",
        "-S",
        "-gdb", f"tcp::{port}",
    ]
    qemu = None
    gdb = None
    qemu_thread = None
    gdb_thread = None
    script_path = None
    qemu_lines: list[str] = []
    gdb_lines: list[str] = []
    started = time.monotonic()

    print(f"[RUN]   {test.build_name}", flush=True)
    if verbose:
        print(f"[QEMU CMD] {display_command(qemu_command)}", flush=True)
        print(f"[GDB] {GDB}", flush=True)
        print(f"[PORT] {port}", flush=True)

    try:
        qemu = subprocess.Popen(
            qemu_command,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        qemu_thread = stream_process(qemu, "[QEMU] ", verbose, qemu_lines)

        script = make_gdb_script(test, port)
        script_directory = ROOT / "obj" / TARGET
        script_directory.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            "w",
            suffix=".gdb",
            prefix="justrt_qemu_",
            dir=script_directory,
            delete=False,
            encoding="utf-8",
        ) as script_file:
            script_file.write(script)
            script_path = Path(script_file.name)

        if verbose:
            print("[GDB SCRIPT] ----------------", flush=True)
            print(script, end="", flush=True)
            print("[GDB SCRIPT] ----------------", flush=True)

        # QEMU creates its listener synchronously during startup. A short delay
        # avoids consuming QEMU's single GDB connection with a probe socket.
        time.sleep(0.15)
        if qemu.poll() is not None:
            raise RuntimeError(
                f"QEMU exited early with code {qemu.returncode}\n"
                + "".join(qemu_lines)
            )

        gdb_command = [GDB, "--batch", "-q", "-x", str(script_path)]
        if verbose:
            print(f"[GDB CMD] {display_command(gdb_command)}", flush=True)
        gdb = subprocess.Popen(
            gdb_command,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        gdb_thread = stream_process(gdb, "[GDB] ", verbose, gdb_lines)

        try:
            gdb.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            elapsed = time.monotonic() - started
            terminate_process(gdb)
            if gdb_thread:
                gdb_thread.join(timeout=1.0)
            tail = "".join(gdb_lines[-30:]).strip()
            details = f"timeout after {elapsed:.2f}s"
            if tail:
                details += "\nGDB output:\n" + tail
            return False, details, elapsed

        if gdb_thread:
            gdb_thread.join(timeout=1.0)
        elapsed = time.monotonic() - started
        output = "".join(gdb_lines)

        if gdb.returncode != 0:
            tail = "\n".join(output.strip().splitlines()[-30:])
            return False, f"GDB exited with code {gdb.returncode}\n{tail}", elapsed

        match = RESULT_RE.search(output)
        if not match:
            tail = "\n".join(output.strip().splitlines()[-40:])
            return False, f"could not read JUSTRT_RESULT\n{tail}", elapsed

        state, runs, passed, failed, done = map(int, match.groups())
        diagnostics = [(name, int(value)) for name, value in DIAG_RE.findall(output)]
        values = dict(diagnostics)
        ok = (
            state == 2
            and passed == 1
            and failed == 0
            and done == 1
            and values.get("g_kernel_invariant_active", 0) == 0
            and values.get("g_fault_active", 0) == 0
            and values.get("g_stack_fault", 0) == 0
        )
        summary = (
            f"state={state} runs={runs} pass={passed} fail={failed} done={done}"
        )
        useful = ", ".join(
            f"{name}={value}" for name, value in diagnostics if value != 0
        )
        if useful:
            summary += " | " + useful
        return ok, summary, elapsed
    finally:
        terminate_process(gdb)
        terminate_process(qemu)
        if gdb_thread:
            gdb_thread.join(timeout=0.5)
        if qemu_thread:
            qemu_thread.join(timeout=0.5)
        if script_path:
            script_path.unlink(missing_ok=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("--quiet-build", action="store_true")
    parser.add_argument("--timeout", type=float, default=TEST_TIMEOUT)
    parser.add_argument(
        "--test",
        choices=[test.build_name for test in TESTS],
        action="append",
        help="run only selected tests; may be repeated",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    for name, command in (("Make", MAKE), ("GDB", GDB), ("QEMU", QEMU)):
        if not executable_exists(command):
            print(f"ERROR: {name} not found: {command}", file=sys.stderr)
            return 2

    selected = [
        test for test in TESTS
        if not args.test or test.build_name in args.test
    ]
    results: list[tuple[str, bool, str, float]] = []

    print("justRT automated QEMU tests")
    print("=" * 56)
    for test in selected:
        try:
            run_build(test, args.verbose, args.quiet_build)
            ok, details, elapsed = run_target(test, args.verbose, args.timeout)
        except subprocess.CalledProcessError as error:
            ok, details, elapsed = (
                False,
                f"build/tool failed with exit code {error.returncode}",
                0.0,
            )
        except Exception as error:
            ok, details, elapsed = False, str(error), 0.0

        results.append((test.build_name, ok, details, elapsed))
        status = "PASS" if ok else "FAIL"
        print(f"[{status}]  {test.build_name:<6} {elapsed:6.2f}s  {details}\n")

    passed_count = sum(1 for _, ok, _, _ in results if ok)
    failed_count = len(results) - passed_count
    print("=" * 56)
    print(f"Result: {passed_count} passed, {failed_count} failed")
    return 0 if failed_count == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
