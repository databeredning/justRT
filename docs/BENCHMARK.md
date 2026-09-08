# Task benchmarking

Benchmarking is optional and currently requires S32K312's DWT cycle counter.
Enable `JRT_ENABLE_TASK_BENCHMARK` and use
`JRT_TASK_DEFINITION_WITH_PERIOD()` when a task has a known period. See the
[kernel reference](RTOS.md) for activation boundaries and snapshot APIs.

From the repository root:

```sh
make -B TEST=benchmark
python tools/run_benchmark.py --flash --runtime 20
```

The runner starts a J-Link GDB server, optionally programs `bin/justrt.hex`,
uses `bin/justrt.elf` for symbols, runs without tick breakpoints, and collects
a report. Without `--flash`, the existing target image must match the ELF.
Use `--verbose` for debugger output. Do not run another J-Link server or
hardware test runner at the same time.

Override local paths with `JRT_BENCHMARK_GDB`, `JRT_BENCHMARK_JLINK_SERVER`,
and `JRT_BENCHMARK_GDB_PORT` (default 2331). The runner uses debug image paths;
build without `BUILD=release` for these commands.

Release/completion counters describe kernel-defined task activations.
Pending work at the instant of capture is not itself a failure. Coalescing
means releases were combined. Maximum activation duration includes elapsed
preemption time; it is not exclusive CPU execution time. Budget compares that
duration with the configured period, and is unavailable without a period.
Stack usage is the lifetime high-water mark. Measurements must remain within
the documented 32-bit cycle-counter wrap limit.

Reports are generated artifacts and should not be committed. For manual
inspection, halt the matching image and inspect `benchmark_records`,
`benchmark_task_count`, and `benchmark_cycle_frequency_hz`; avoid breakpoints
in the scheduler or tick path while measuring.
