#!/usr/bin/env python3
"""Hardware-in-the-loop test runner for justrt using SEGGER J-Link + GDB.

Builds each TEST variant, starts JLinkGDBServerCL, flashes bin/justrt.elf,
runs the target, and stops on a hardware watchpoint when test_result.done changes.

Configuration can be overridden with environment variables:
  JUSTRT_MAKE            path/name of make or mingw32-make
  JUSTRT_GDB             path/name of arm-none-eabi-gdb
  JUSTRT_JLINK_SERVER    path/name of JLinkGDBServerCL.exe
  JUSTRT_TEST_TIMEOUT    per-test runtime timeout in seconds (default: 5)
  JUSTRT_GDB_PORT        GDB server TCP port (default: 2331)
"""

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
ELF = ROOT / "bin" / "justrt.elf"

DEFAULT_GDB = "C:/devtools/gcc/gcc-10.2-arm32-eabi/bin/arm-none-eabi-gdb.exe"
DEFAULT_JLINK = "C:/Program Files/SEGGER/JLink/JLinkGDBServerCL.exe"

MAKE = os.environ.get("JUSTRT_MAKE")
if not MAKE:
    MAKE = shutil.which("make") or shutil.which("mingw32-make") or "make"
GDB = os.environ.get("JUSTRT_GDB", DEFAULT_GDB)
JLINK = os.environ.get("JUSTRT_JLINK_SERVER", DEFAULT_JLINK)
GDB_PORT = int(os.environ.get("JUSTRT_GDB_PORT", "2331"))
TEST_TIMEOUT = float(os.environ.get("JUSTRT_TEST_TIMEOUT", "5"))


@dataclass(frozen=True)
class TestCase:
    build_name: str
    result_expr: str
    diagnostic_exprs: tuple[str, ...] = ()
    expected_fault_type: int = 0
    expected_fault_address_expr: str = ""


TESTS = (
    TestCase(
        "boot",
        "g_test_boot_and_privilege",
        ("g_test_boot_argument",),
    ),
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
        "fpu",
        "g_test_fpu.result",
        (
            "g_test_fpu.task_a_checks",
            "g_test_fpu.task_b_checks",
            "g_test_fpu.non_fp_runs",
            "g_test_fpu.error_code",
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
    TestCase(
        "timer_service",
        "g_test_timer_service.result",
        (
            "g_test_timer_service.one_shot_callbacks",
            "g_test_timer_service.periodic_callbacks",
            "g_test_timer_service.accumulated_first_callbacks",
            "g_test_timer_service.accumulated_second_callbacks",
            "g_test_timer_service.restart_callbacks",
            "g_test_timer_service.starter_callbacks",
            "g_test_timer_service.target_callbacks",
            "g_test_timer_service.polling_expirations",
            "g_test_timer_service.compatibility_callbacks",
            "g_test_timer_service.context_checks",
            "g_test_timer_service.callback_task_index",
            "g_test_timer_service.error_code",
        ),
    ),
    TestCase(
        "task_capacity",
        "g_test_task_capacity.result",
        (
            "g_test_task_capacity.configured_limit",
            "g_test_task_capacity.tasks_ran",
            "g_test_task_capacity.maximum_accepted",
            "g_test_task_capacity.maximum_plus_one_rejected",
            "g_test_task_capacity.guard_updates",
            "g_test_task_capacity.guard_base_matches",
            "g_test_task_capacity.ready_scan_depth",
            "g_test_task_capacity.scheduler_pass2_max",
            "g_test_task_capacity.error_code",
        ),
    ),
    TestCase(
        "private_config",
        "g_test_private_config.result",
        (
            "g_test_private_config.invalid_cases_rejected",
            "g_test_private_config.valid_config_accepted",
            "g_test_private_config.tasks_ran",
            "g_test_private_config.task_a_value",
            "g_test_private_config.task_b_value",
            "g_test_private_config.error_code",
        ),
    ),
    TestCase(
        "stack_guard",
        "g_test_stack_guard.result",
        (
            "g_test_stack_guard.expected_guard_address",
            "g_test_stack_guard.write_attempted",
        ),
        expected_fault_type=2,
        expected_fault_address_expr=
            "g_test_stack_guard.expected_guard_address",
    ),
)

RESULT_RE = re.compile(
    r"JUSTRT_RESULT state=(\d+) runs=(\d+) pass=(\d+) fail=(\d+) done=(\d+)"
)
DIAG_RE = re.compile(r"JUSTRT_DIAG ([^=]+)=(\d+)")
EXPECTED_FAULT_RE = re.compile(
    r"JUSTRT_EXPECTED_FAULT type=(\d+) mmfar=(\d+) expected=(\d+) "
    r"cfsr=(\d+) task=(\d+)"
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
    "g_mpu_stack_guard_base",
    "g_mpu_stack_guard_updates",
    "g_mpu_private_data_base",
    "g_mpu_private_data_size",
    "g_mpu_private_data_updates",
)


def executable_exists(command: str) -> bool:
    p = Path(command)
    return p.exists() if p.is_absolute() or p.parent != Path(".") else shutil.which(command) is not None



def quote_for_display(value: object) -> str:
    return '"' + str(value).replace('"', r'\"') + '"'


def display_command(cmd: list[object]) -> str:
    return " ".join(quote_for_display(x) for x in cmd)


def stream_process(proc, prefix: str, verbose: bool, lines: list[str]) -> threading.Thread:
    def worker():
        if proc.stdout is None:
            return
        for line in proc.stdout:
            lines.append(line)
            if verbose:
                print(prefix + line, end="", flush=True)

    t = threading.Thread(target=worker, daemon=True)
    t.start()
    return t


def run_build(test: TestCase, verbose: bool, quiet_build: bool) -> None:
    cmd = [MAKE, "-B"]
    if quiet_build:
        cmd.append("-s")
    cmd.append(f"TEST={test.build_name}")
    print(f"[BUILD] {test.build_name}", flush=True)
    if verbose:
        print(f"[BUILD CMD] {display_command(cmd)}", flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)


def wait_for_port(proc: subprocess.Popen[str], port: int, timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            out = proc.stdout.read() if proc.stdout else ""
            raise RuntimeError(f"J-Link GDB server exited early (code {proc.returncode}).\n{out}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise TimeoutError(f"J-Link GDB server did not open port {port}")


def start_jlink(verbose: bool):
    cmd = [
        JLINK,
        "-select", "USB",
        "-device", "S32K312",
        "-if", "JTAG",
        "-speed", "auto",
        "-port", str(GDB_PORT),
    ]
    if not verbose:
        cmd.append("-silent")

    if verbose:
        print(f"[JLINK CMD] {display_command(cmd)}", flush=True)

    # Do not embed literal quotes in argv elements. subprocess handles spaces
    # in paths such as "C:/Program Files/..." correctly.
    proc = subprocess.Popen(
        cmd,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    lines: list[str] = []
    thread = stream_process(proc, "[JLINK] ", verbose, lines)

    # Give J-Link time to initialize without opening a dummy TCP connection.
    time.sleep(1.0)

    if proc.poll() is not None:
        thread.join(timeout=0.5)
        raise RuntimeError(
            f"J-Link exited early with code {proc.returncode}\n" + "".join(lines)
        )

    return proc, thread, lines

def gdb_script(test: TestCase, verbose: bool) -> str:
    r = test.result_expr
    lines = [
        "set pagination off",
        "set confirm off",
        "set breakpoint pending off",
    ]
    if verbose:
        lines += ["set verbose on", "show architecture"]
    lines += [
        f'file "{ELF.as_posix()}"',
        f"target remote 127.0.0.1:{GDB_PORT}",

        # Download the image first.
        "load",

        # Install the completion watchpoint before reset/start. Startup may
        # write done=0; the GDB condition filters those writes automatically.
        f"watch {r}.done",
        f"condition $bpnum {r}.done != 0",
        "watch g_kernel_invariant_active",
        "condition $bpnum g_kernel_invariant_active != 0",
        "watch g_fault_active",
        "condition $bpnum g_fault_active != 0",
        'printf "JUSTRT: completion watchpoint installed\\n"',

        # J-Link GDB Server accepts "monitor reset"; avoid the OpenOCD-style
        # "monitor reset halt", which produced syntax errors on this setup.
        "monitor reset",
        'printf "JUSTRT: target reset; continuing\\n"',
        "continue",

    ]
    if test.expected_fault_type != 0:
        lines.extend([
            "if g_fault_active == 0",
            '  printf "JUSTRT_UNEXPECTED_STOP pc=%p\\n", $pc',
            "else",
            f'  printf "JUSTRT_EXPECTED_FAULT type=%u mmfar=%u expected=%u cfsr=%u task=%u\\n", g_fault_record.fault_type, g_fault_record.mmfar, {test.expected_fault_address_expr}, g_fault_record.cfsr, g_current_task_index',
        ])
    else:
        lines.extend([
            # If execution stops for some unrelated reason, report it clearly.
            f"if {r}.done == 0",
            '  printf "JUSTRT_UNEXPECTED_STOP pc=%p\\n", $pc',
            '  printf "JUSTRT_INVARIANT active=%u code=%u task=%u object=%u aux=%u tick=%u\\n", g_kernel_invariant_active, g_kernel_invariant_code, g_kernel_invariant_task, g_kernel_invariant_object, g_kernel_invariant_aux, g_kernel_invariant_tick',
            '  printf "JUSTRT_FAULT active=%u\\n", g_fault_active',
            "  x/i $pc",
            "  info registers pc lr sp xpsr",
            "  bt",
            "else",
            f'  printf "JUSTRT_RESULT state=%u runs=%u pass=%u fail=%u done=%u\\n", {r}.state, {r}.runs, {r}.pass, {r}.fail, {r}.done',
        ])
    for expr in test.diagnostic_exprs + KERNEL_DIAGNOSTICS:
        lines.append(f'  printf "JUSTRT_DIAG {expr}=%u\\n", {expr}')
    lines.extend(["end", "monitor halt", "quit"])
    return "\n".join(lines) + "\n"


def terminate_process(proc: subprocess.Popen[str]) -> None:
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=1.0)


def run_target(test: TestCase, verbose: bool, timeout: float) -> tuple[bool, str, float]:
    print(f"[RUN]   {test.build_name}", flush=True)

    if verbose:
        print(f"[ELF]   {quote_for_display(ELF.as_posix())}", flush=True)
        print(f"[GDB]   {quote_for_display(GDB)}", flush=True)
        print(f"[JLINK] {quote_for_display(JLINK)}", flush=True)
        print(f"[PORT]  {GDB_PORT}", flush=True)
        print(f"[TIMEOUT] {timeout:.1f}s", flush=True)

    server = None
    server_thread = None
    gdb = None
    gdb_thread = None
    script_path = None
    started = time.monotonic()
    gdb_lines: list[str] = []

    try:
        server, server_thread, _ = start_jlink(verbose)

        script = gdb_script(test, verbose)

        script_dir = ROOT / "obj"
        script_dir.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            "w", suffix=".gdb", prefix="justrt_", dir=script_dir,
            delete=False, encoding="utf-8"
        ) as f:
            f.write(script)
            script_path = Path(f.name)

        gdb_script_path = script_path.relative_to(ROOT).as_posix()

        if verbose:
            print("[GDB SCRIPT] ----------------", flush=True)
            print(script, end="", flush=True)
            print("[GDB SCRIPT] ----------------", flush=True)

        cmd = [GDB, "--batch", "-q", "-x", gdb_script_path]

        if verbose:
            print(f"[GDB CMD] {display_command(cmd)}", flush=True)

        gdb = subprocess.Popen(
            cmd,
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
            if verbose:
                print(f"[TIMEOUT] test exceeded {timeout:.1f}s", flush=True)
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

        diagnostics = [(name, int(value)) for name, value in DIAG_RE.findall(output)]
        diagnostic_values = dict(diagnostics)
        if test.expected_fault_type != 0:
            fault_match = EXPECTED_FAULT_RE.search(output)
            if not fault_match:
                tail = "\n".join(output.strip().splitlines()[-40:])
                return False, f"could not read expected fault result\n{tail}", elapsed
            fault_type, mmfar, expected, cfsr, task_id = map(
                int, fault_match.groups()
            )
            ok = (
                fault_type == test.expected_fault_type
                and mmfar == expected
                and (cfsr & 0x82) == 0x82
                and diagnostic_values.get(
                    "g_test_stack_guard.write_attempted", 0
                ) == 1
                and diagnostic_values.get("g_kernel_invariant_active", 0) == 0
            )
            summary = (
                f"fault_type={fault_type} mmfar={mmfar} expected={expected} "
                f"cfsr={cfsr} task={task_id}"
            )
            useful = ", ".join(
                f"{name}={value}" for name, value in diagnostics if value != 0
            )
            if useful:
                summary += " | " + useful
            return ok, summary, elapsed

        match = RESULT_RE.search(output)
        if not match:
            tail = "\n".join(output.strip().splitlines()[-40:])
            if "JUSTRT_UNEXPECTED_STOP" in output:
                return False, "target stopped before test completion\n" + tail, elapsed
            return False, f"could not read JUSTRT_RESULT\n{tail}", elapsed

        state, runs, passed, failed, done = map(int, match.groups())
        ok = (done != 0 and state == 2 and failed == 0
              and diagnostic_values.get("g_kernel_invariant_active", 0) == 0
              and diagnostic_values.get("g_fault_active", 0) == 0
              and diagnostic_values.get("g_stack_fault", 0) == 0)
        summary = f"state={state} runs={runs} pass={passed} fail={failed} done={done}"

        useful = ", ".join(f"{name}={value}" for name, value in diagnostics if value != 0)
        if useful:
            summary += " | " + useful

        return ok, summary, elapsed

    finally:
        terminate_process(gdb)
        terminate_process(server)
        if gdb_thread:
            gdb_thread.join(timeout=0.5)
        if server_thread:
            server_thread.join(timeout=0.5)
        if script_path:
            script_path.unlink(missing_ok=True)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="stream J-Link and GDB output")
    parser.add_argument("--timeout", type=float, default=TEST_TIMEOUT,
                        help=f"per-test timeout in seconds (default: {TEST_TIMEOUT:g})")
    parser.add_argument("--quiet-build", action="store_true",
                        help="suppress nested make build output")
    parser.add_argument("--test", choices=[t.build_name for t in TESTS],
                        action="append",
                        help="run only selected test; may be repeated")
    return parser.parse_args()

def main() -> int:
    args = parse_args()
    if not executable_exists(MAKE):
        print(f"ERROR: Make not found: {MAKE}", file=sys.stderr)
        print("Set JUSTRT_MAKE to make.exe or mingw32-make.exe.", file=sys.stderr)
        return 2
    if not executable_exists(GDB):
        print(f"ERROR: GDB not found: {GDB}", file=sys.stderr)
        print("Set JUSTRT_GDB to the correct arm-none-eabi-gdb executable.", file=sys.stderr)
        return 2
    if not executable_exists(JLINK):
        print(f"ERROR: J-Link GDB server not found: {JLINK}", file=sys.stderr)
        print("Set JUSTRT_JLINK_SERVER to JLinkGDBServerCL.exe.", file=sys.stderr)
        return 2

    print("JustRT automated target tests")
    print("=" * 56)
    if args.verbose:
        print("[VERBOSE] enabled", flush=True)
        print(f"[ROOT]  {quote_for_display(ROOT.as_posix())}", flush=True)
        print(f"[MAKE]  {quote_for_display(MAKE)}", flush=True)

    selected = [t for t in TESTS if not args.test or t.build_name in args.test]
    results: list[tuple[str, bool, str, float]] = []

    for test in selected:
        try:
            run_build(test, args.verbose, args.quiet_build)
            ok, details, elapsed = run_target(test, args.verbose, args.timeout)
        except subprocess.CalledProcessError as exc:
            ok, details, elapsed = False, f"build/tool failed with exit code {exc.returncode}", 0.0
        except Exception as exc:
            ok, details, elapsed = False, str(exc), 0.0

        results.append((test.build_name, ok, details, elapsed))
        status = "PASS" if ok else "FAIL"
        print(f"[{status}]  {test.build_name:<6} {elapsed:6.2f}s  {details}")
        print()

    print("=" * 56)
    passed_count = sum(1 for _, ok, _, _ in results if ok)
    failed_count = len(results) - passed_count
    print(f"Result: {passed_count} passed, {failed_count} failed")
    return 0 if failed_count == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
