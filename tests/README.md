# Testing justRT

Run commands from the repository root. Tests require Python 3, GNU Make,
GNU Arm Embedded GCC, and GDB. QEMU tests require `qemu-system-arm`; hardware
tests require an S32K312 board and SEGGER J-Link and will flash the board.
On Windows use Git Bash (or set Make's SHELL to Git's sh.exe). Override the
runner defaults with `JUSTRT_MAKE`, `JUSTRT_GDB`, `JUSTRT_QEMU`,
`JUSTRT_JLINK_SERVER`, and `JUSTRT_SIZE` as appropriate. Set `PYTHON=python`
for Make targets if the default `py` launcher is unavailable.

```sh
make config-test
make qemu-test
make auto-test
make qemu-test BUILD=release
make auto-test BUILD=release
python tools/run_qemu_tests.py --quiet-build --test boot --test sync
python tools/run_tests.py --quiet-build --test fpu --test stack_guard
make TEST=mutex TARGET=qemu-mps2-an385
```

Builds and runners default to debug mode. Make test targets accept
`BUILD=release`; when invoking a Python runner directly, use `--build release`,
for example:

```sh
python tools/run_qemu_tests.py --quiet-build --build release --test sync
```

See [build modes](../README.md#build) for compiler flags and image paths.

`TEST=simple` is the default example. Other profiles use `tests/main.c` and
compile only their selected test implementation. Objects are isolated by
profile, target, and optimization; image paths stay stable for debugger tools.
Use `-B` to force a rebuild after overriding compiler settings on the command line.

| Profiles | Coverage | Target |
| --- | --- | --- |
| `boot` | Startup, arguments, privilege gateways | Both |
| `config_runtime`, `task_capacity`, `private_config` | Configuration and task definitions | Both |
| `fatal_hook`, `fatal_hook_return` | Fatal hook and fallback halt | Both |
| `sync`, `mutex` | Synchronization and priority inheritance | Both |
| `race`, `stress` | Wakeups, timeouts, tick wrap; stress extends the same test | Both |
| `timer_service` | Callback dispatch and timer lifecycle | Both |
| `task_suspension` | Suspend/resume and preserved context | Both |
| `fpu` | Floating-point register preservation | S32K312 |
| `stack_guard`, `mpu_isolation_read`, `mpu_isolation_write`, `task_suspension_mpu` | Expected protection faults | S32K312 |
| `benchmark` | Optional task benchmark instrumentation | S32K312 |

Runners without `--test` execute their full suite, including stress. Use
repeated `--test` options for focused work. Run configuration checks and
relevant tests for small changes; exercise scheduling, synchronization, and
timers in optimized builds for shared kernel changes. FPU and MPU behavior
requires hardware validation. No formal release acceptance record is required.

Tests report explicit pass/fail state and kernel diagnostics. Protection
profiles pass only when their expected fault is captured. The continuous
example is not a terminating regression test. An intermittent QEMU
`timer_service` error 12 (an extra periodic callback) has also reproduced on
the baseline before the repository cleanups; investigate failures rather than
silently retrying them in CI.

CI builds both targets and runs configuration checks plus the QEMU suite in
an optimized build. Tool versions are printed for diagnosis. Hardware testing
remains local.
