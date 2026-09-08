# justRT

justRT is a small, statically configured preemptive RTOS. It is freestanding,
uses no heap or C runtime, and provides priority scheduling, task delays,
semaphores, recursive mutexes with priority inheritance, queues, notifications,
event groups, software timers, and fixed-size memory pools.

Supported targets:

- NXP S32K312 Cortex-M7, with floating-point context support, MPU stack guards,
  and optional unprivileged tasks with private-data isolation.
- QEMU MPS2-AN385 Cortex-M3 for kernel regression and experimentation.

## Build

Use GNU Make and the GNU Arm Embedded toolchain (`arm-none-eabi-gcc`). On
Windows, run Make with Git Bash or another shell supporting `mkdir -p`.

```sh
make
make TARGET=qemu-mps2-an385
make BUILD=release
```

The default application is [the simple example](examples/README.md).
The build modes select compiler optimization, independently of any release process:

| Mode | Compiler flags | Intended use |
| --- | --- | --- |
| `BUILD=debug` (default) | `-Og -g3` | Development and easier debugger stepping |
| `BUILD=release` | `-O2 -g3 -DNDEBUG` | Optimized execution and timing validation |

Both retain debugger symbols and kernel fault/invariant diagnostics. In the
optimized build, some variables may be unavailable and stepping may not follow
source order.
Application settings are supplied through `JRTConfig.h`; see below.

S32K312 images are written to `bin/`, QEMU images to
`bin/qemu-mps2-an385/`, with `release/` subdirectories for optimized builds.
For example, S32K312 ELF paths are `bin/justrt.elf` (debug) and
`bin/release/justrt.elf` (release). QEMU uses
`bin/qemu-mps2-an385/justrt.elf` and
`bin/qemu-mps2-an385/release/justrt.elf`. Use the ELF matching the build when
attaching a debugger. Objects are separated by target, build mode, and test
profile under `obj/`; switching modes does not require cleaning.

```sh
qemu-system-arm -M mps2-an385 -cpu cortex-m3 -kernel bin/qemu-mps2-an385/justrt.elf -nographic -S -gdb tcp::1234
```

Attach GDB on port 1234 and continue, or use the VS Code QEMU launch profile.
Exit QEMU with `Ctrl+A`, then `X`.

## Application configuration

**Each application should own its `JRTConfig.h`.** The
[header bundled here](JRTConfig.h) provides example/test defaults, including
an S32K312 clock; these are not recommended settings for every application.

For an application integrating justRT:

1. Copy `JRTConfig.h` into the application's configuration directory.
2. Set the actual core clock, tick rate, task capacity, priorities, stack
   sizes, and optional diagnostics for that application.
3. In the application's build, put that directory before the justRT root
   in the include path for every kernel, architecture, and application C
   source, for example `-Iapp/config -Ipath/to/justRT`.
4. Rebuild all sources when changing the configuration or its include path.
   Every source in an image must use the same configuration.

The repository Makefile uses the bundled header. For experiments in this
repository, edit that header; a separate application build should use its
own copy. Individual settings can also be overridden with compiler `-D`
options because the defaults are guarded by `#ifndef`. In a custom header,
retain these guards if compiler overrides are wanted. Avoid contradictory
clock definitions in the application header and target build flags.

Choose stack sizes from measured worst-case usage plus margin; compile-time
minimums only ensure room for the architecture context. CPU capabilities
such as FPU and MPU support are selected by the architecture/target build,
not by application preference. See the [kernel reference](docs/RTOS.md) for
configuration constraints.

## Development

```sh
make config-test
make qemu-test
make auto-test
make qemu-test BUILD=release
```

The hardware runner flashes the S32K312 through J-Link. See the
[testing guide](tests/README.md) for prerequisites and individual profiles.

## Layout and documentation

- `kernel/`: scheduling, synchronization, timers, memory pools, and diagnostics.
- `arch/cortex_m/`: CPU context, exceptions, privilege, and fault handling.
- `platform/`: target startup, linker layouts, clocks, and board support.
- `examples/`: application entry point and simple example.
- `tests/`: regression firmware and profile selection.
- `tools/`: local build, debugger, test, and benchmark runners.
- [Application integration guide](docs/INTEGRATION.md)
- [Kernel API and execution contract](docs/RTOS.md)
- [Cortex-M architecture and platform guide](docs/PORT.md)
- [Benchmark collection](docs/BENCHMARK.md)
- [S32K312 board notes](platform/s32k312/board/README.md)
