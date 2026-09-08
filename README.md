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
  unprivileged task execution with per-task private-data isolation.
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
make -B TEST=fatal_hook
make -B TEST=fatal_hook_return
make -B TEST=sync
make -B TEST=mutex
make -B TEST=fpu
make -B TEST=race
make -B TEST=timer_service
make -B TEST=benchmark
make -B TEST=task_capacity
make -B TEST=stack_guard
make -B TEST=private_config
make -B TEST=mpu_isolation_read
make -B TEST=mpu_isolation_write
make -B TEST=config_runtime
make config-test
make qemu-release-test
make auto-release-test
python tools/run_benchmark.py --duration-ticks 750 --output benchmark.md
```

`BUILD=debug` uses `-Og`; `BUILD=release` uses `-O2 -DNDEBUG`. Their objects
and output images are kept in separate directories so changing optimization
cannot reuse stale objects. The two release-test targets run the
optimization-sensitive synchronization, extended stress/tick-wrap,
timer-service, and task-suspension profiles on QEMU or S32K312. The hardware
target also verifies expected stack-guard and suspended-private-data MPU
faults separately from unexpected fatal diagnostics. Release runs print ELF
text, data, BSS, and total sizes.

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
race, task-capacity, task-suspension, dynamic-guard, and private-data mapping
diagnostics, including stack high-water and critical-section counts. The
`stack_guard`, `mpu_isolation_read`, `mpu_isolation_write`,
and `task_suspension_mpu` profiles pass by capturing their expected MemManage
faults. The continuous `simple` example is intentionally not part of the
terminating test suite.

The automated QEMU runner builds and verifies the terminating `boot`,
`config_runtime`, `fatal_hook`, `fatal_hook_return`, `sync`, `mutex`, `race`, `stress`,
`timer_service`, `task_capacity`, `private_config`, and `task_suspension`
profiles, including result, fault, invariant, stack, scheduler,
MPU-transition, and tick diagnostics:

```sh
make qemu-test
```

The Cortex-M3 target does not support `TEST=fpu`, `TEST=stack_guard`, the two
`TEST=mpu_isolation_*` hardware profiles, or `TEST=task_suspension_mpu`. See
[TODO.md](TODO.md) for the remaining work.

Task benchmarking currently requires the S32K312 DWT cycle counter and is
therefore rejected for the QEMU Cortex-M3 target. The benchmark profile and
`tools/run_benchmark.py` use the S32K312 hardware runner; QEMU remains useful
for testing the ordinary kernel regression suite.

## Repository layout

- `kernel/` — scheduler, synchronization, timers, memory pools, and fatal policy.
- `arch/cortex_m/` — architecture contract, task context, exceptions, and CPU fault handling.
- `platform/s32k312/` — S32K312 startup, memory layout, vectors, and board
  support.
- `platform/qemu_mps2_an385/` — QEMU Cortex-M3 startup, memory layout, vectors,
  and board support.
- `examples/` — board-independent application examples.
- `tests/` — named debugger-driven regression firmware.
- `tools/` — build, flash, GDB, and test automation.

## Documentation

- [Kernel, API, execution-context, and interrupt-safety reference](RTOS.md)
- [Architecture and platform porting guide](PORT.md)
- [Supported release configuration and acceptance checklist](RELEASE.md)
- [Current roadmap](TODO.md)
- [Example application guide](examples/README.md)
- [S32K312 board notes](platform/s32k312/board/README.md)
