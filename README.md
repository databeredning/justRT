# justRT

justRT is a small, statically configured preemptive real-time operating
system. It is freestanding, uses no heap or C runtime, and emphasizes
deterministic scheduling, privilege separation, and debugger-visible
diagnostics.

The portable kernel provides priority-based preemptive scheduling, task
delays, binary semaphores, recursive mutexes with priority inheritance,
bounded queues, task notifications, event groups, software timers, and fixed
size memory pools. Synchronization and timeout paths are hardened against lost
wakeups and tick-counter wraparound, while fail-stop invariant checks preserve
diagnostic state for post-failure inspection.

## Targets

- NXP S32K312 Cortex-M7 hardware, including MPU stack guards and optional
  unprivileged task execution.
- QEMU MPS2-AN385 Cortex-M3 for fast scheduler and kernel regression work.

The Cortex-M exception path uses PSP for tasks and MSP for kernel and exception
handling. SVC starts the first task and provides privileged gateways, while
PendSV performs context switching and SysTick drives timekeeping.

## Build

Builds use the GNU Arm Embedded toolchain and GNU Make. The S32K312 target is
the default:

```sh
make -B TEST=simple
make -B TEST=boot
make -B TEST=sync
make -B TEST=mutex
make -B TEST=fpu
make -B TEST=race
make -B TEST=timer_service
```

Build the QEMU Cortex-M3 target with:

```sh
make -B TARGET=qemu-mps2-an385 TEST=simple
qemu-system-arm -M mps2-an385 -cpu cortex-m3 -kernel bin/qemu-mps2-an385/justrt.elf -nographic -S -gdb tcp::1234
```

QEMU starts paused at reset and listens for GDB on TCP port 1234. Select
`QEMU: Attach justRT` in VS Code and start debugging to continue execution.

To stop QEMU in `-nographic` mode, press `Ctrl+A`, release the keys, and then
press `X`.

Build artifacts are kept separately under `bin/` and `obj/` so switching
targets cannot reuse objects compiled for another CPU.

## Configuration

Application and target-specific settings are collected in `JRTConfig.h`.
This header configures the core clock, kernel tick rate, maximum number of
application tasks, and default application and idle stack sizes. Kernel-owned
task capacity and architecture-derived limits remain private to the kernel.

## Hardware regression tests

The automated S32K312 runner builds, flashes, and verifies the named tests
through J-Link and GDB:

```sh
make auto-test
```

Successful tests expose explicit `state`, `pass`, `fail`, and `done` fields.
The runner also reports fault, invariant, scheduler, synchronization, timer,
and race diagnostics. The continuous `simple` example is intentionally not
part of the terminating test suite.

The automated QEMU runner builds and verifies the terminating `boot`, `sync`,
`mutex`, `race`, and `timer_service` profiles, including result, fault,
invariant, stack, scheduler, and tick diagnostics:

```sh
make qemu-test
```

The Cortex-M3 target does not support `TEST=fpu`. See [TODO.md](TODO.md) for
the remaining cross-target validation work.

## Repository layout

- `kernel/` — scheduler, synchronization, timers, memory pools, and Cortex-M
  exception implementation.
- `arch/cortex_m/` — architecture contract used by the portable kernel.
- `platform/s32k312/` — S32K312 startup, memory layout, vectors, and board
  support.
- `platform/qemu_mps2_an385/` — QEMU Cortex-M3 startup, memory layout, vectors,
  and board support.
- `examples/` — board-independent application examples.
- `tests/` — named debugger-driven regression firmware.
- `tools/` — build, flash, GDB, and test automation.

## Documentation

- [Kernel and API reference](RTOS.md)
- [Architecture and platform porting guide](PORT.md)
- [Current roadmap](TODO.md)
- [Example application guide](examples/README.md)
- [S32K312 board notes](platform/s32k312/board/README.md)
