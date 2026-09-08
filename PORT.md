# Porting justRT to a New Architecture or Platform

This guide describes the boundary between the portable kernel, Cortex-M
support, and target-specific startup and board code. It also records the
implemented QEMU MPS2-AN385 port as a reference.

## Project Layers

```text
kernel/            Scheduler, synchronization, timers, and memory pools.
arch/<family>/     Architecture contract consumed by the portable kernel.
platform/<target>/ Reset startup, vectors, linker script, and board driver.
```

Portable `kernel/*.c` code must not access CPU or board registers directly.
The Cortex-M exception implementation lives under `arch/cortex_m/`, while
`arch/cortex_m/port_contract.h` defines the boundary that portable code calls. A new board using the same Cortex-M model
normally needs a new `platform/<target>/` and build selection, not a fork of
the portable kernel.

## Cortex-M Port Contract

The portable kernel uses these functions from
`arch/cortex_m/port_contract.h`:

| Function | Responsibility |
| --- | --- |
| `arch_build_initial_stack()` | Construct the initial Cortex-M task context; context-size constants also belong to the port contract. |
| `arch_disable_interrupts()` | Disable interrupts for fatal handling without changing critical-section diagnostics. |
| `arch_halt()` | Stop execution permanently if the fatal hook returns. |
| `arch_request_switch()` | Pend the lowest-priority context-switch exception. |
| `arch_critical_enter()` | Mask interrupts and return the previous mask state. |
| `arch_critical_exit()` | Restore the exact state returned by critical entry. |
| `arch_in_isr()` | Report whether execution is in exception context. |
| `arch_tick_init()` | Configure SysTick and exception priorities without starting interrupts. |
| `arch_tick_start()` | Start the configured tick from protected first-task startup. |
| `arch_yield()` | Enter the privileged yield path and reschedule. |
| `arch_configure_mpu()` | Install static protection regions and the first task's dynamic stack guard and private-data region. |
| `arch_set_task_stack_guard()` | Replace the dynamic guard with the selected task's guard before exception return. |
| `arch_set_task_private_data()` | Replace or disable the selected task's dynamic private-data region before exception return. |
| `arch_start_first_task()` | Enter startup SVC and restore the first task context. |
| `arch_wait_for_interrupt()` | Wait efficiently in the idle task. |

The contract also defines the CONTROL values `ARCH_LAUNCH_PRIVILEGED` and
`ARCH_LAUNCH_UNPRIVILEGED`. Scheduler capacity is independent of MPU region
count: the port maintains one dynamic guard for the currently selected task,
while the kernel retains software stack-bound checks on targets without MPU
support.

## Exception and Context Rules

Tasks use PSP. Reset, kernel code, SVC, PendSV, SysTick, and fault handlers use
MSP. Startup enters SVC 0, which restores the synthetic task frame through
`arch_restore_task_context()` and exception-returns using PSP.

PendSV saves the outgoing `EXC_RETURN` and `r4-r11`, asks the scheduler for the
next saved stack pointer, restores that task, reapplies its CONTROL value, and
exception-returns. When `JRT_ARCH_FPU_CONTEXT=1`, the path also preserves
`s16-s31` for tasks whose `EXC_RETURN` identifies an extended FP frame.

The relevant implementation files are:

- `arch/cortex_m/port_cm7.c`: Cortex-M register access, SysTick, PendSV, SVC dispatch,
  and optional MPU programming.
- `arch/cortex_m/svc_cm7.s`: SVC entry and shared task-context restore.
- `arch/cortex_m/svc_stubs_cm7.c`: task-facing unprivileged SVC wrappers.
- `arch/cortex_m/fault.c`: debugger-visible fault capture and fail-stop handlers.

The `cm7` filenames are historical. The same code is compiled for the current
Cortex-M3 QEMU target with FPU and MPU features disabled.

## MPU and Linker Contract

When `JRT_ARCH_HAS_MPU=1`, the target supplies linker-aligned regions for
privileged flash and SRAM, unprivileged code, SVC wrappers, read-only data,
task data, and a 32-byte dynamic task stack guard. Higher-numbered Cortex-M
MPU regions win overlaps, so the guard region must override the general SRAM
mapping.

Task-private objects occupy a separate `.task_private_data` output section.
The linker must define `__task_private_data_start` and
`__task_private_data_end`, align the section to at least 32 bytes, and keep it
outside the statically shared unprivileged data mapping. Each individual
object has power-of-two size and matching alignment so the dynamic MPU region
can represent it exactly. The current Cortex-M allocation uses region 14 for
private data and region 15 for the stack guard. Both are updated for the first
task and on every selection; private region 14 is disabled when the selected
task has no private object.

The minimum supported private region is 32 bytes. Cortex-M region sizes are
powers of two and the base must be aligned to the selected size. Ports must
not silently round a private range outward: that could expose a neighboring
object. Keep explicitly shared writable objects in `.unprivileged_task_data`,
keep task-owned aggregates in `.task_private_data`, and reject any private
definition that cannot be mapped exactly or overlaps another owned range.
Dynamic region use is constant regardless of configured task count: one slot
represents the selected task's private data and one represents its guard.

When `JRT_ARCH_HAS_MPU=0`, MPU register programming is omitted. Stack bounds
are still checked in software and dynamic guard ownership remains visible in
diagnostics, but the target does not provide privilege-based memory isolation.

The linker script must retain these sections:

| Source attribute | Output section |
| --- | --- |
| `KERNEL_PRIVILEGED` | `.privileged_functions` |
| `KERNEL_PRIVILEGED_DATA` | `.privileged_data` |
| `JRT_TASK_UNPRIVILEGED` | `.unprivileged_functions` |
| `JRT_TASK_UNPRIVILEGED_RODATA` | `.unprivileged_rodata` |
| `JRT_TASK_UNPRIVILEGED_DATA` | `.unprivileged_task_data` |
| `JRT_TASK_PRIVATE_DATA(size)` | `.task_private_data` |
| Unprivileged SVC wrappers | `.unprivileged_svc` |

The SVC handler itself remains privileged. MPU-enabled ports must also define
`__unprivileged_task_data_start`, `__unprivileged_task_data_end`,
`__task_private_data_start`, and `__task_private_data_end`. Startup must
provide whatever symbols its `.data` copy and `.bss` clear implementation
uses.

## Board Contract

Each `platform/<target>/board/` provides:

```c
void board_init(void);
void board_led_toggle(void);
```

Both functions are privileged. An unprivileged task calls
`JRT_BoardLedToggle()`, whose SVC wrapper reaches the privileged board driver.
The QEMU board implementation is intentionally a no-op because `simple` has no
peripheral dependency and regression results are inspected through GDB.

## Build Integration

The root Makefile selects a target with `TARGET`. A new target must provide:

- its platform include path and source/object rules;
- CPU flags matching the core and floating-point ABI;
- a linker script and separate target artifact directories;
- feature definitions for `JRT_ARCH_HAS_MPU` and `JRT_ARCH_FPU_CONTEXT`;
- an explicit policy for tests that require unavailable hardware features.

Keep each target's objects separate so code compiled for one CPU cannot be
silently reused for another. The current layouts are `obj/s32k312`,
`bin/s32k312`, `obj/qemu-mps2-an385`, and `bin/qemu-mps2-an385`.

## Implemented QEMU Reference Target

The QEMU target lives in `platform/qemu_mps2_an385/` and contains:

```text
startup_cm3.s       Reset handler and data/BSS initialization.
Vector_Table.s      Cortex-M exception vector table.
linker.ld           MPS2-AN385 flash and SRAM layout.
system.c            Target system initialization.
board/board.c       Privileged no-op board implementation.
board/board.h       Board interface declarations.
```

It uses Cortex-M3 flags, `JRT_ARCH_HAS_MPU=0`, and
`JRT_ARCH_FPU_CONTEXT=0`. Build and run it from Git Bash/MINGW64 with:

```sh
make -B TARGET=qemu-mps2-an385 TEST=simple
qemu-system-arm -M mps2-an385 -cpu cortex-m3 -kernel bin/qemu-mps2-an385/justrt.elf -nographic -S -gdb tcp::1234
```

QEMU starts paused at reset and listens for GDB on TCP port 1234. Select
`QEMU: Attach justRT` in VS Code and start debugging to continue execution.

To stop QEMU in `-nographic` mode, press `Ctrl+A`, release the keys, and then
press `X`.

Manual bring-up has confirmed increasing
ticks and context switches, execution of both `simple` tasks, and no recorded
fault or invariant failure.

`make qemu-test` automatically builds and verifies the terminating `boot`,
`sync`, `mutex`, and `race` profiles through QEMU/GDB. `TEST=fpu` is rejected
for this target because Cortex-M3 has no FPU.

## Validation Checklist

For a new target, validate in increasing scope:

1. Build `TEST=simple` and confirm first-task startup, both application tasks,
   delays, yields, SysTick, PendSV, and idle execution.
2. Run `TEST=boot` and confirm task arguments, privilege transitions, and the
   SVC board gateway. Treat MPU isolation as target-dependent.
3. Run `TEST=sync` for ISR semaphore, queue, notification, and event-group
   paths.
4. Run `TEST=mutex` for recursive ownership and priority inheritance.
5. Run `TEST=race` for lost-wakeup, timeout, wraparound, mutex, queue, and timer
   start/stop/restart races.
6. Run `TEST=fpu` only when the target enables floating-point context support.
7. Confirm `g_fault_active == 0`, `g_kernel_invariant_active == 0`,
   `g_stack_fault == 0`, and increasing `g_context_switches`.

On S32K312 hardware, `make auto-test` runs the terminating `boot`, `sync`,
`mutex`, `fpu`, and `race` profiles through J-Link/GDB. Use
`py tools/run_tests.py --verbose` for detailed runner output.
