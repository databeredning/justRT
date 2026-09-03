# Supported Release Configuration

This document defines the release candidate configuration for justRT. A tag
is qualified only after the acceptance record at the end of this document has
been completed from a clean checkout.

## Tool and Target Baseline

The automated release baseline is Ubuntu 24.04 with Python 3.12, GNU Arm
Embedded GCC 13.2.1, QEMU 8.2, and GDB 15. The manually triggered
`Release validation` GitHub workflow rejects a different major/minor tool
family and archives the exact versions used by each run. Patch releases of
Python, QEMU, and GDB may vary within those pinned families; the compiler is
pinned to 13.2.1.

The supported targets are:

- `s32k312`: NXP S32K312 Cortex-M7, hard-float ABI, MPU and floating-point
  context support enabled. Runtime acceptance requires the physical board and
  SEGGER J-Link.
- `qemu-mps2-an385`: QEMU MPS2-AN385 Cortex-M3, without MPU or floating-point
  context support. This target provides repeatable functional regression but
  does not replace the hardware acceptance run.

Only the root Makefile target combinations and linker scripts in this
repository are part of this release configuration. A new CPU, compiler ABI,
linker layout, or optimization policy requires separate port validation.

## Configuration Envelope

Production settings are selected in `JRTConfig.h`. The checked constraints
are summarized below; the default values are the release reference values.

| Setting | Default | Supported constraint |
| --- | ---: | --- |
| `JRT_CORE_CLOCK_HZ` | 120000000 | Nonzero, fits `uint32_t` |
| `JRT_TICK_RATE_HZ` | 7500 | Nonzero, no greater than the core clock, produces a 24-bit SysTick reload |
| `JRT_MAX_APPLICATION_TASKS` | 7 | Nonzero and representable with three reserved scheduler slots |
| `JRT_DEFAULT_TASK_STACK_WORDS` | 128 | Even, no smaller than the architecture context, byte size fits `uint32_t` |
| `JRT_IDLE_STACK_WORDS` | 128 | Same stack constraints as application stacks |
| `JRT_TIMER_SERVICE_STACK_WORDS` | 128 | Same stack constraints as application stacks |
| `JRT_MAX_TASK_PRIORITY` | 31 | Nonzero and fits `uint32_t` |
| `JRT_TIMER_SERVICE_PRIORITY` | 1 | Above idle priority and no greater than the configured maximum |
| `JRT_ENABLE_TEST_HOOKS` | 0 | Either 0 or 1; production release uses 0 |
| `JRT_ENABLE_TASK_BENCHMARK` | 0 | Either 0 or 1; production release uses 0 |

Passing compile-time checks proves that a value is representable, not that it
is suitable for a particular application. The application must size every
stack from measured high-water use plus interrupt and call-depth margin,
account for the scheduler's linear scan as task capacity grows, and select a
tick rate consistent with its latency and CPU-load budget.

Release firmware uses `BUILD=release`, which selects `-O2 -g3 -DNDEBUG` and
keeps debugger information without disabling kernel invariant, stack, fault,
or fatal-path diagnostics.

## Application Integration Checklist

Before treating an application image as production-ready:

1. Keep task definitions, stacks, queues, synchronization objects, timers,
   and memory pools statically allocated for the lifetime expected by their
   APIs.
2. Give every unprivileged task only its required private aggregate. Place
   deliberately shared objects in `JRT_TASK_UNPRIVILEGED_DATA`; never store a
   secret in shared unprivileged memory.
3. Confirm task priorities, timer-service priority, blocking relationships,
   and worst-case callback work. Timer callbacks must remain bounded and must
   not block.
4. Measure stack high-water values under application worst-case load and add
   explicit safety margin. Do not use the architectural minimum as an
   application sizing recommendation.
5. Implement `JRT_FatalErrorHook()` only if the product needs persistent
   logging, watchdog handoff, or reset. The hook must be bounded and safe with
   maskable interrupts disabled.
6. Verify the target clock before starting the kernel and keep interrupt
   priorities consistent with the execution and ISR contract in `RTOS.md`.
7. Review all DMA and peripheral bus-master access separately; the CPU MPU
   does not protect private task data from those agents.
8. Build with `BUILD=release`, review the ELF size and map file, and retain the
   matching ELF for fault diagnosis.
9. Run the complete QEMU release workflow and the complete S32K312 hardware
   suite from the exact source revision to be tagged.

## Residual Risks and Unsupported Features

- There is no dynamic task creation or deletion. Suspension supports only
  running and ready application tasks and does not cancel active waits.
- Faults are fail-stop. Recovery, reset, persistent logging, and watchdog
  policy belong to the application and platform.
- MPU enforcement is available only on MPU-enabled targets. Privileged code,
  DMA, and other bus masters can bypass task-private CPU access controls.
- Each task has at most one private power-of-two region with matching base
  alignment. General process-style address spaces are not provided.
- Software-timer callbacks execute serially in one privileged service task;
  a callback that fails to return prevents later callbacks from running.
- Scheduler selection and several object wait-list operations are linear in
  configured task count. Larger task limits need application-specific timing
  measurement.
- QEMU does not validate S32K312 MPU behavior, floating-point context,
  startup timing, interrupt integration, peripherals, flash programming, or
  electrical behavior.
- The kernel has extensive deterministic regression coverage but is not a
  certified safety kernel and has no claimed compliance with a functional
  safety standard.

## Release Acceptance Record

Run from a clean checkout of the exact candidate revision:

```sh
make config-test
python tools/run_qemu_tests.py --quiet-build --build release
make auto-test
make auto-release-test
```

The GitHub `Release validation` workflow covers the configuration tests, both
release builds, and the complete QEMU release suite. The two hardware commands
must be run locally with the S32K312 and J-Link connected. Record the source
revision, exact tool versions, workflow artifact, hardware result, and final
ELF sizes in the release notes before creating the milestone tag.
