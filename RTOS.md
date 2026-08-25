# justRT

## Scope

justRT is a statically configured preemptive RTOS for the NXP S32K312
Cortex-M7. It uses no heap or C runtime. The current port uses PSP for tasks,
MSP for reset and exception handlers, SysTick for timekeeping, and PendSV for
context switching.

## Source Layout

- `kernel/`: portable scheduler, synchronization, timers, and memory pools.
- `arch/cortex_m/`: architecture contract.
- `kernel/port_cm7.c`: Cortex-M7 port, MPU, SVC dispatch, SysTick, and PendSV.
- `kernel/svc_cm7.s`: SVC exception handler and context restore helper.
- `kernel/svc_stubs_cm7.c`: unprivileged SVC wrappers.
- `platform/s32k312/`: startup, vectors, linker script, and board driver.
- `examples/`: hardware regression profiles.

## Task Model

Tasks are supplied statically through `kernel_config_t`:

```c
static const task_definition_t tasks[] = {
    { entry, argument, stack_words, priority, "name", flags }
};
```

The kernel supports up to seven application tasks plus one idle task. States
are `READY`, `RUNNING`, `SLEEPING`, and `BLOCKED`. Higher numeric priorities
run first; equal priorities are selected round-robin.

Stacks are 128 words by default. Each stack has an aligned 32-byte MPU guard.
The initial frame contains eight software-saved registers followed by the
standard eight-word Cortex-M hardware frame:

```text
r4-r11 | r0 r1 r2 r3 r12 LR PC xPSR
```

The task argument is restored in `r0`. A returning task enters
`task_exit_trap()`.

## Startup and Context Switching

`kernel_start()` enables SysTick and executes startup SVC 0. The SVC handler:

1. Loads the current task's saved stack pointer.
2. Restores `r4-r11` with `arch_restore_task_context()`.
3. Sets PSP and the task's CONTROL value.
4. Returns with PSP `EXC_RETURN`.

The processor restores the hardware frame and enters the task. Later PendSV
saves the outgoing `r4-r11`, calls `pendsv_switch()`, restores the selected
task through the same helper, updates CONTROL, and returns through the saved
exception return value.

The stack split is:

```text
MSP: reset, kernel code, SVC, SysTick, PendSV, and fault handlers
PSP: task code and task stacks
```

## SVC ABI

SVC wrappers are placed in `.unprivileged_svc`; exception handling remains
privileged.

| Number | Service |
|---:|---|
| 0 | Start first task; MSP caller only |
| 1 | Yield and request PendSV |
| 2 | Sleep for the tick count in `r0` |
| 3 | Toggle the board LED through privileged code |

Normal task SVC calls require Thread mode using PSP. The SVC handler reads the
number from the instruction before the stacked PC and rejects invalid context
or service numbers.

## MPU

`arch_configure_mpu()` clears all region slots and installs this static map:

| Region | Contents | Access |
|---:|---|---|
| 0 | Base internal flash | Privileged read/execute only |
| 1 | Base internal SRAM | Privileged read/write, XN |
| 2 | `.unprivileged_functions` | Read/execute both privilege levels |
| 3 | `.unprivileged_svc` | Read/execute both privilege levels |
| 4 | `.unprivileged_rodata` | Read-only, XN |
| 5 | `.unprivileged_task_data` | Read/write, XN |
| 8-15 | Per-task stack guards | No access, XN |

Higher region numbers override lower ones. The linker aligns the explicit
unprivileged sections to MPU-compatible boundaries. `PRIVDEFENA` remains set
for the privileged background map. Task privilege is selected by
`TASK_FLAG_UNPRIVILEGED` and reapplied by PendSV.

## Kernel Services

- Binary semaphores with task and ISR give operations.
- Recursive mutexes with priority inheritance and chain restoration.
- Bounded queues with task send/receive and non-blocking ISR send.
- Per-task accumulated notifications.
- Event groups with wait-any, wait-all, and clear-on-exit options.
- One-shot and periodic software timers. SysTick records expirations; task
  code dispatches callbacks.
- Fixed-size memory pools protected by critical sections.

Task waits use tick timeouts; `SEMAPHORE_WAIT_FOREVER` is the infinite timeout
value. Synchronization APIs intended for tasks reject ISR use, while dedicated
ISR APIs are non-blocking.

## Port Boundary

Portable kernel code calls the contract in
`arch/cortex_m/port_contract.h` for critical sections, ISR detection, tick
startup, yielding, MPU setup, first-task startup, context switching, and idle
wait. Board-specific GPIO access stays in
`platform/s32k312/board/board.c` and is reached from unprivileged tasks through
the LED SVC gateway.

## Build and Validation

```sh
make -B TEST=simple
make -B TEST=boot
make -B TEST=sync
make -B TEST=mutex
make auto-test
```

`make auto-test` invokes `tools/run_tests.py` and builds, flashes, and runs
the boot, synchronization, and mutex tests through J-Link/GDB. It suppresses
nested build output while preserving test status and diagnostics. The runner
also accepts `--quiet-build`, `--verbose`, `--timeout`, and repeated
`--test <name>` options.

Tests:

- `simple`: continuous board-independent task switching example.
- `boot`: unprivileged startup, MPU, SVC LED gateway, and privilege switching.
- `sync`: ISR semaphore, queue, event-group, and notification paths.
- `mutex`: recursive ownership, priority inheritance, and chained waiters.

Useful diagnostics include `g_fault_record`, `g_fault_active`,
`g_context_switches`, `g_kernel_ticks`, `g_svc_invalid_service`, and
`g_svc_invalid_context`.

## Current Limitations

- Static task configuration only; no task creation, deletion, suspend, or
  resume API.
- Task data is shared between unprivileged tasks; per-task MPU isolation is not
  implemented.
- MPU regions are static and use power-of-two ranges.
- Fault handling records state and stops; it does not recover or reset.
- Timer callbacks require explicit task-side dispatch.
- Synchronization waits and timer list operations still require further
  concurrency hardening.
