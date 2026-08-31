# justRT

## Scope

justRT is a statically configured preemptive RTOS for Cortex-M targets. It
currently supports the NXP S32K312 Cortex-M7 and QEMU's MPS2-AN385 Cortex-M3.
It uses no heap or C runtime. Tasks use PSP, reset and exception handlers use
MSP, SysTick provides timekeeping, and PendSV performs context switching.

## Source Layout

- `kernel/`: portable scheduler, synchronization, timers, and memory pools.
- `arch/cortex_m/`: architecture contract.
- `kernel/port_cm7.c`: shared Cortex-M port, optional MPU, SVC dispatch,
  SysTick, and PendSV. The historical filename is retained.
- `kernel/svc_cm7.s`: SVC exception handler and context restore helper.
- `kernel/svc_stubs_cm7.c`: unprivileged SVC wrappers.
- `platform/s32k312/`: startup, vectors, linker script, and board driver.
- `platform/qemu_mps2_an385/`: QEMU startup, vectors, linker script, and board
  driver.
- `examples/`: board-independent examples.
- `tests/`: named regression firmware profiles.

## Task Model

Tasks are supplied statically through `JRT_KernelConfig_t`:

```c
JRT_DECLARE_STATIC_TASK_STACK(worker_stack, 256U);

static const JRT_TaskDefinition_t tasks[] = {
    JRT_TASK_DEFINITION(entry, argument, worker_stack, priority, "name", flags)
};
```

The public configuration supports up to `JRT_MAX_APPLICATION_TASKS` (seven)
application tasks. Scheduler storage privately reserves three additional
slots for kernel-owned tasks; currently only the idle task is created
internally. The S32K312 MPU assigns regions 6-15 to ten stack guards, matching
the total scheduler capacity.
States are `READY`, `RUNNING`, `SLEEPING`, and `BLOCKED`. Higher numeric
priorities run first; equal priorities are selected round-robin.

Each task supplies a statically allocated stack whose size is selected by the
application. `JRT_DEFAULT_TASK_STACK_WORDS` is 128 words for applications that
do not need a custom size. `JRT_DECLARE_STATIC_TASK_STACK()` places an aligned
32-byte MPU guard immediately below the stack and
`JRT_TASK_DEFINITION()` registers both with the kernel. The kernel owns the
idle-task stack and still performs no heap allocation.

Application and target-specific build settings live in `JRTConfig.h`. They
select the core clock, tick rate, application-task limit, default application
stack size, and idle-task stack size. Kernel-owned task capacity and derived
architecture limits remain private implementation details.

Kernel initialization rejects null, undersized, oddly sized, misaligned,
non-adjacent, overflowing, or overlapping stack/guard ranges. Stack sizes are
specified in 32-bit words and must be even so the initial exception frame has
the required 8-byte alignment.

The initial frame contains a saved `EXC_RETURN`, eight software-saved
registers, and the standard eight-word Cortex-M hardware frame:

```text
EXC_RETURN | r4-r11 | r0 r1 r2 r3 r12 LR PC xPSR
```

The saved task-context pointer is 4-byte aligned because `EXC_RETURN` adds one
word ahead of `r4-r11`. After the software context is restored, the resulting
hardware PSP is again 8-byte aligned as required by the exception-return ABI.

With `JRT_ARCH_FPU_CONTEXT` enabled, the architectural minimum is 51 words:
17 for the basic context, 16 for `s16-s31`, and 18 for the hardware FP frame.
Normal C call depth, local variables, and interrupt headroom require additional
space beyond this minimum.

The task argument is restored in `r0`. A returning task enters
`task_exit_trap()`.

## Startup and Context Switching

`JRT_KernelStart()` configures SysTick and executes startup SVC 0. The SVC
handler starts the tick while SVC still masks the lower-priority SysTick and
PendSV exceptions, then restores the first task. This prevents a context
switch before PSP has been initialized. The SVC handler:

Before issuing SVC 0, the port normalizes `CONTROL` to privileged Thread mode
using MSP with FPCA clear. This prevents PSP or floating-point state used by
pre-scheduler application code from changing the bootstrap exception frame.

1. Loads the current task's saved stack pointer.
2. Restores the task's saved `EXC_RETURN` and `r4-r11` with
   `arch_restore_task_context()`.
3. Sets PSP and the task's CONTROL value.
4. Returns with PSP `EXC_RETURN`.

The processor restores the hardware frame and enters the task. Later PendSV
saves the outgoing `EXC_RETURN` and `r4-r11`, calls `pendsv_switch()`, restores
the selected task through the same helper, updates CONTROL, and returns through
its saved exception return value.

When `JRT_ARCH_FPU_CONTEXT` is enabled, PendSV also saves and restores
`s16-s31` when `EXC_RETURN` bit 4 indicates that the task owns an extended
floating-point exception frame. Tasks that have not used floating point keep
the basic frame and do not incur this additional context cost.

Before the scheduler starts, the Cortex-M port enables CP10/CP11 and automatic
lazy floating-point stacking (`FPCCR.ASPEN` and `FPCCR.LSPEN`). During task
restore, `CONTROL.FPCA` is set only when the selected task's saved
`EXC_RETURN` identifies an extended floating-point frame.

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
or service numbers. MSP/PSP points at the core-register frame for both basic
and extended floating-point exception frames; the additional hardware FP
registers occupy the higher-address portion of an extended frame.

## MPU

The S32K312 target enables the MPU and installs this static map:

`arch_configure_mpu()` clears all region slots and installs this static map:

| Region | Contents | Access |
|---:|---|---|
| 0 | Base internal flash | Privileged read/execute only |
| 1 | Base internal SRAM | Privileged read/write, XN |
| 2 | `.unprivileged_functions` | Read/execute both privilege levels |
| 3 | `.unprivileged_svc` | Read/execute both privilege levels |
| 4 | `.unprivileged_rodata` | Read-only, XN |
| 5 | `.unprivileged_task_data` | Read/write, XN |
| 6-15 | Per-task stack guards | No access, XN |

Higher region numbers override lower ones. The linker aligns the explicit
unprivileged sections to MPU-compatible boundaries. `PRIVDEFENA` remains set
for the privileged background map. Task privilege is selected by
`JRT_TASK_FLAG_UNPRIVILEGED` and reapplied by PendSV. The QEMU Cortex-M3 target
currently builds with `JRT_ARCH_HAS_MPU=0`; its stack guards therefore rely on
the kernel's software bounds checks and do not validate hardware isolation.

## Kernel Services

- Binary semaphores with task and ISR give operations.
- Recursive mutexes with priority inheritance and chain restoration.
- Bounded queues with task send/receive and non-blocking ISR send.
- Per-task accumulated notifications.
- Event groups with wait-any, wait-all, and clear-on-exit options.
- One-shot and periodic software timers. SysTick records expirations; task
  code dispatches callbacks. Timer configuration and expiry processing are
  serialized by the kernel critical section. If expiry wins a race with stop,
  start, or restart, that expiration remains pending while the later operation
  controls the timer's next deadline.
- Fixed-size memory pools protected by critical sections.

Task waits convert relative tick timeouts to one absolute deadline when the API
is entered. The deadline is retained across internal retry loops. Expiry uses
unsigned elapsed-tick arithmetic, which remains valid across 32-bit tick
wraparound for every finite timeout value.
`JRT_WAIT_FOREVER` selects an infinite wait. Synchronization APIs intended for
tasks reject ISR use, while dedicated ISR APIs are non-blocking.

## Port Boundary

Portable kernel code calls the contract in
`arch/cortex_m/port_contract.h` for critical sections, ISR detection, tick
startup, yielding, MPU setup, first-task startup, context switching, and idle
wait. Board-specific access stays below each `platform/<target>/board/`
directory and is reached from unprivileged tasks through the LED SVC gateway.

## Build and Validation

```sh
make -B TEST=simple
make -B TEST=boot
make -B TEST=sync
make -B TEST=mutex
make -B TEST=fpu
make -B TEST=race
make auto-test
```

`make auto-test` invokes `tools/run_tests.py` and builds, flashes, and runs
the boot, synchronization, mutex, FPU, and race tests on S32K312 hardware
through J-Link/GDB. It suppresses nested build output while preserving test
status and diagnostics. The runner also accepts `--quiet-build`, `--verbose`,
`--timeout`, and repeated `--test <name>` options.

Build and launch the QEMU target with:

```sh
make -B TARGET=qemu-mps2-an385 TEST=simple
qemu-system-arm -M mps2-an385 -cpu cortex-m3 -kernel bin/qemu-mps2-an385/justrt.elf -nographic -S -gdb tcp::1234
```

QEMU starts paused at reset and listens for GDB on TCP port 1234. Select
`QEMU: Attach justRT` in VS Code and start debugging to continue execution.

To stop QEMU in `-nographic` mode, press `Ctrl+A`, release the keys, and then
press `X`.

`make qemu-test` runs the terminating `boot`, `sync`, `mutex`, and `race`
profiles under QEMU/GDB and checks their results plus fault, invariant, stack,
scheduler, and tick diagnostics. `TEST=fpu` is intentionally unavailable for
the Cortex-M3 CPU.

Tests:

- `simple`: continuous board-independent task switching example.
- `boot`: unprivileged startup, MPU, SVC LED gateway, and privilege switching.
- `sync`: ISR semaphore, queue, event-group, and notification paths.
- `mutex`: recursive ownership, priority inheritance, and chained waiters.
- `fpu`: FP-to-FP and FP-to-non-FP context switches across SVC and SysTick.
- `race`: blocking, timeout, tick-wrap, timer start/stop/restart, and wake-up
  race coverage.

Useful diagnostics include `g_fault_record`, `g_fault_active`,
`g_context_switches`, `g_kernel_ticks`, `g_svc_invalid_service`, and
`g_svc_invalid_context`. Kernel invariant failures set
`g_kernel_invariant_active` and record the invariant code, task, object,
auxiliary value, and tick before stopping with interrupts masked.

## Current Limitations

- Static task configuration only; no task creation, deletion, suspend, or
  resume API.
- Task data is shared between unprivileged tasks; per-task MPU isolation is not
  implemented.
- MPU regions are static and use power-of-two ranges.
- Fault handling records state and stops; it does not recover or reset.
- Timer callbacks require explicit task-side dispatch.
- QEMU cannot exercise MPU isolation or floating-point context switching.
