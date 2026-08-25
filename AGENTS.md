# justRT Agent Guide

## Project

justRT is a small statically configured preemptive RTOS for the NXP S32K312
Cortex-M7. It is freestanding and does not use a heap or C runtime.

Current target layout:

- `kernel/`: portable scheduler, synchronization, timers, and memory pools.
- `arch/cortex_m/`: architecture contract used by the portable kernel.
- `kernel/port_cm7.c`: Cortex-M7 port, MPU, SysTick, PendSV, and SVC dispatch.
- `kernel/svc_cm7.s`: SVC exception entry and shared task-context restore.
- `kernel/svc_stubs_cm7.c`: unprivileged SVC wrappers.
- `platform/s32k312/`: reset startup, vectors, linker script, and board driver.
- `examples/simple.c`: continuous board-independent task-switching example.
- `tests/`: named hardware regression tests.
- `tools/run_tests.py`: J-Link/GDB hardware test runner.

Read `RTOS.md` for the concise implementation reference and `PORT.md` for
architecture or platform porting rules.

## Build and Test

Use the Windows Python launcher in this workspace:

```sh
make -B TEST=simple
make -B TEST=boot
make -B TEST=sync
make -B TEST=mutex
make auto-test
```

`make auto-test` runs `tools/run_tests.py --quiet-build`, which builds,
flashes, and verifies `boot`, `sync`, and `mutex` through J-Link/GDB. For
verbose runner output:

```sh
py tools/run_tests.py --verbose
```

Runner configuration is provided with environment variables:
`JUSTRT_GDB`, `JUSTRT_JLINK_SERVER`, `JUSTRT_TEST_TIMEOUT`, and
`JUSTRT_GDB_PORT`.

The default `TEST=simple` image is not part of the automated runner because it
runs indefinitely. It is intended for scheduler observation.

## Architecture Rules

- Keep CPU register access in the Cortex-M port; portable `kernel/*.c` code
  calls the architecture contract.
- Tasks use PSP. Reset, kernel code, and exception handlers use MSP.
- First-task startup enters SVC 0. The handler restores the synthetic task
  frame and returns with PSP `EXC_RETURN`.
- PendSV saves the outgoing `r4-r11`, selects the next task, restores its
  context, applies its CONTROL privilege, and exception-returns.
- SVC wrappers are unprivileged and live in `.unprivileged_svc`.
  `SVC_Handler` and service implementations remain privileged.
- Do not let unprivileged task code call privileged functions directly. Add an
  SVC wrapper and dispatch service when a new task-facing operation is needed.
- Keep `arch_restore_task_context()` usable from both startup SVC and PendSV.
- SVC service IDs are an ABI: `0` startup, `1` yield, `2` sleep, `3` LED.

## MPU Rules

The S32K312 port uses 16 MPU regions:

- Region 0: privileged-only base flash.
- Region 1: privileged-only base SRAM, XN.
- Region 2: unprivileged functions.
- Region 3: unprivileged SVC wrappers.
- Region 4: unprivileged read-only data.
- Region 5: unprivileged task data, XN.
- Regions 8-15: 32-byte no-access task stack guards.

Higher region numbers win overlaps. Linker section boundaries used as MPU
regions must be aligned to the selected power-of-two region size. Avoid broad
full-access regions that defeat privilege isolation. `PRIVDEFENA` is currently
enabled for the privileged background map.

## Test Conventions

Tests run in fresh firmware images because the kernel has no task deletion or
reset API. Keep test state debugger-visible and use explicit pass/fail/done
fields. A task must not return: park completed tasks in a loop instead of
falling into `task_exit_trap()`.

The expected successful test result is:

```text
state == 2
pass  == 1
fail  == 0
done  == 1
```

Do not combine expected-fault MPU tests with passing tests; the current fault
handler records the fault and intentionally stops.

## Editing and Validation

Keep changes focused and preserve existing user changes. Before committing:

1. Build the affected named test.
2. Run `git diff --check`.
3. For runtime changes, run `make auto-test` or the applicable hardware test.
4. Inspect `g_fault_active`, `g_fault_record`, `g_context_switches`, and test
   result state in the debugger.
5. Use concise commit prefixes already used by the project, such as `arch:`,
   `docs:`, and `test:`.

Do not commit generated `bin/` or `obj/` artifacts unless the repository policy
explicitly changes.
