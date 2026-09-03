#!/usr/bin/env python3
"""Compile-only checks for accepted and rejected justRT configurations."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CC = os.environ.get("JUSTRT_CC", shutil.which("arm-none-eabi-gcc") or "arm-none-eabi-gcc")
SOURCE = '#include "kernel.h"\nint config_probe(void) { return 0; }\n'


@dataclass(frozen=True)
class ConfigCase:
    name: str
    definitions: tuple[str, ...]
    expected_error: str = ""


CASES = (
    ConfigCase("default", ()),
    ConfigCase(
        "supported_overrides",
        (
            "JRT_CORE_CLOCK_HZ=80000000UL",
            "JRT_TICK_RATE_HZ=1000UL",
            "JRT_MAX_APPLICATION_TASKS=12U",
            "JRT_DEFAULT_TASK_STACK_WORDS=160U",
            "JRT_IDLE_STACK_WORDS=96U",
            "JRT_TIMER_SERVICE_STACK_WORDS=192U",
            "JRT_MAX_TASK_PRIORITY=15U",
            "JRT_TIMER_SERVICE_PRIORITY=2U",
        ),
    ),
    ConfigCase("zero_core_clock", ("JRT_CORE_CLOCK_HZ=0UL",), "must be greater than zero"),
    ConfigCase("zero_tick_rate", ("JRT_TICK_RATE_HZ=0UL",), "must be greater than zero"),
    ConfigCase(
        "tick_above_core",
        ("JRT_CORE_CLOCK_HZ=1000UL", "JRT_TICK_RATE_HZ=1001UL"),
        "must not exceed",
    ),
    ConfigCase(
        "systick_reload_overflow",
        ("JRT_CORE_CLOCK_HZ=120000000UL", "JRT_TICK_RATE_HZ=1UL"),
        "24-bit hardware limit",
    ),
    ConfigCase("zero_task_limit", ("JRT_MAX_APPLICATION_TASKS=0U",), "must be greater than zero"),
    ConfigCase(
        "task_limit_index_overflow",
        ("JRT_MAX_APPLICATION_TASKS=4294967295ULL",),
        "scheduler index capacity",
    ),
    ConfigCase("short_default_stack", ("JRT_DEFAULT_TASK_STACK_WORDS=50U",), "architecture minimum"),
    ConfigCase("odd_default_stack", ("JRT_DEFAULT_TASK_STACK_WORDS=127U",), "must be even"),
    ConfigCase("short_idle_stack", ("JRT_IDLE_STACK_WORDS=50U",), "architecture minimum"),
    ConfigCase("odd_idle_stack", ("JRT_IDLE_STACK_WORDS=127U",), "must be even"),
    ConfigCase("oversized_idle_stack", ("JRT_IDLE_STACK_WORDS=1073741824U",), "byte size overflows"),
    ConfigCase("short_timer_stack", ("JRT_TIMER_SERVICE_STACK_WORDS=50U",), "architecture minimum"),
    ConfigCase("odd_timer_stack", ("JRT_TIMER_SERVICE_STACK_WORDS=127U",), "must be even"),
    ConfigCase(
        "oversized_timer_stack",
        ("JRT_TIMER_SERVICE_STACK_WORDS=1073741824U",),
        "byte size overflows",
    ),
    ConfigCase("zero_max_priority", ("JRT_MAX_TASK_PRIORITY=0U",), "must be greater than zero"),
    ConfigCase("zero_timer_priority", ("JRT_TIMER_SERVICE_PRIORITY=0U",), "greater than idle priority"),
    ConfigCase(
        "timer_priority_above_max",
        ("JRT_MAX_TASK_PRIORITY=15U", "JRT_TIMER_SERVICE_PRIORITY=16U"),
        "exceeds JRT_MAX_TASK_PRIORITY",
    ),
    ConfigCase("invalid_test_hook_setting", ("JRT_ENABLE_TEST_HOOKS=2U",), "must be 0 or 1"),
    ConfigCase(
        "benchmark_enabled",
        ("JRT_ENABLE_TASK_BENCHMARK=1U",),
    ),
    ConfigCase(
        "invalid_benchmark_setting",
        ("JRT_ENABLE_TASK_BENCHMARK=2U",),
        "must be 0 or 1",
    ),
    ConfigCase(
        "benchmark_unsupported_target",
        ("JRT_ENABLE_TASK_BENCHMARK=1U", "JRT_ARCH_HAS_DWT_CYCCNT=0U"),
        "requires a supported cycle counter",
    ),
)


def run_case(case: ConfigCase) -> tuple[bool, str]:
    command = [
        CC,
        "-mcpu=cortex-m7",
        "-mthumb",
        "-DJRT_ARCH_FPU_CONTEXT=1",
        "-DJRT_ARCH_HAS_MPU=1",
        "-DJRT_ARCH_HAS_DWT_CYCCNT=1",
        "-ffreestanding",
        "-fsyntax-only",
        "-x",
        "c",
        "-",
        f"-I{ROOT}",
        f"-I{ROOT / 'kernel'}",
    ]
    command.extend(f"-D{definition}" for definition in case.definitions)
    result = subprocess.run(command, input=SOURCE, text=True, capture_output=True)
    output = result.stdout + result.stderr
    if case.expected_error:
        ok = result.returncode != 0 and case.expected_error in output
    else:
        ok = result.returncode == 0
    return ok, output.strip()


def main() -> int:
    failures = 0

    print("justRT configuration compile tests")
    print("=" * 40)
    for case in CASES:
        ok, output = run_case(case)
        print(f"[{'PASS' if ok else 'FAIL'}] {case.name}")
        if not ok:
            failures += 1
            if output:
                print(output)
    print("=" * 40)
    print(f"Result: {len(CASES) - failures} passed, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
