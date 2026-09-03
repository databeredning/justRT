# justRT Benchmark Debugging

This document describes how to collect the native justRT task benchmark from
an S32K312 target. The benchmark uses the Cortex-M DWT cycle counter and the
kernel-owned task benchmark records.

## Target prerequisites

The firmware must be built with:

```c
#define JRT_ENABLE_TASK_BENCHMARK 1U
#define JRT_ARCH_HAS_DWT_CYCCNT 1U
```

Tasks that should report a period must use
`JRT_TASK_DEFINITION_WITH_PERIOD()` and provide the period in kernel ticks.
The image containing these settings must be flashed before collecting data.

The benchmark does not flash the target. The ELF is loaded by GDB for symbols,
while the program already in target flash is reset and executed.

## Connect to hardware

Start the J-Link GDB server for the target, for example:

```sh
JLinkGDBServerCL.exe -select USB -device S32K312 -if JTAG -speed auto -port 2331
```

Use the same GDB server port in the commands below. Keep only one server
connected to the target.

Start the ARM GDB client with the ELF that matches the flashed image:

```sh
arm-none-eabi-gdb.exe -q path/to/justrt.elf
```

At the GDB prompt, connect without loading or programming the image:

```gdb
target remote 127.0.0.1:2331
```

## Manual collection

Stop at kernel initialization so the benchmark state can be checked after the
kernel has initialized its task table:

```gdb
set confirm off
file path/to/justrt.elf
target remote 127.0.0.1:2331
tbreak JRT_KernelInit
monitor reset
continue
finish
p/u benchmark_cycle_counter_available
p/u benchmark_cycle_frequency_hz
```

The expected values are `1` for availability and a non-zero core frequency.
The `finish` command returns to the application after `JRT_KernelInit()` has
completed. At this point the target is stopped.

Start measurement without a breakpoint in the tick handler:

```gdb
continue
```

Leave the target running for the required duration. When the duration has
elapsed, press `Ctrl-C` in GDB. Wait until GDB reports that the target stopped,
then retrieve the records:

```gdb
p/u benchmark_cycle_counter_available
p/u benchmark_cycle_frequency_hz
p/u benchmark_task_count
set $i = 0
while $i < benchmark_task_count
  printf "id=%u name=%s releases=%u completions=%u coalesced=%u release_max=%u activation_max=%u period=%u flags=%u stack=%u used=%u\\n", $i, tasks[$i].name, benchmark_records[$i].release_count, benchmark_records[$i].completion_count, benchmark_records[$i].coalesced_count, benchmark_records[$i].max_release_latency_cycles, benchmark_records[$i].max_activation_cycles, benchmark_records[$i].period_cycles, tasks[$i].flags, tasks[$i].stack_top - tasks[$i].stack_bottom, tasks[$i].high_water_words
  set $i = $i + 1
end
monitor halt
quit
```

Do not issue the `printf` commands until GDB has stopped the target. Sending
commands while the target is still running produces `Cannot execute this
command while the target is running`.

## Native fields

The benchmark record is `JRT_TaskBenchmarkRecord_t` in `kernel/benchmark.h`.
The public snapshot API is:

```c
JRT_BenchmarkGetInfo(&info);
JRT_BenchmarkGetTask(task_id, &task_info);
```

The output fields correspond to:

| Output | Native source | Description |
|---|---|---|
| `releases` | `release_count` | Recorded task releases |
| `completions` | `completion_count` | Completed activations |
| `coalesced` | `coalesced_count` | Releases combined into another activation |
| `release_max` | `max_release_latency_cycles` | Maximum release-to-activation latency |
| `activation_max` | `max_activation_cycles` | Maximum elapsed activation duration |
| `period` | `period_cycles` | Configured task period in cycles |
| `flags` | `tasks[id].flags` | Native task flags |
| `stack` | `tasks[id].stack_top - tasks[id].stack_bottom` | Configured stack size in words |
| `used` | `tasks[id].high_water_words` | Stack high-water usage in words |

Pending releases are calculated as:

```text
pending = release_count - completion_count - coalesced_count
```

Cycle values can be converted on the host using `benchmark_cycle_frequency_hz`:

```text
microseconds = cycles * 1000000 / benchmark_cycle_frequency_hz
```

The timer-service task is included after the application tasks. It normally
has `period=0` because it is an internal kernel task.

## Small Python example

This example assumes that the J-Link GDB server is already running and that
`arm-none-eabi-gdb.exe` is available. It uses external Python for timing, so it
does not require GDB embedded-Python support.

```python
import subprocess
import time

GDB = r"C:\devtools\gcc\gcc-10.2-arm32-eabi\bin\arm-none-eabi-gdb.exe"
ELF = r"C:\path\to\justrt.elf"
PORT = "2331"
SECONDS = 20

startup = f'''file "{ELF.replace(chr(92), "/")}"
set confirm off
target remote 127.0.0.1:{PORT}
tbreak JRT_KernelInit
monitor reset
continue
finish
p/u benchmark_cycle_counter_available
p/u benchmark_cycle_frequency_hz
continue&
'''

report = '''interrupt
p/u benchmark_cycle_counter_available
p/u benchmark_cycle_frequency_hz
p/u benchmark_task_count
set $i = 0
while $i < benchmark_task_count
  printf "id=%u name=%s releases=%u completions=%u coalesced=%u release_max=%u activation_max=%u period=%u flags=%u stack=%u used=%u\\n", $i, tasks[$i].name, benchmark_records[$i].release_count, benchmark_records[$i].completion_count, benchmark_records[$i].coalesced_count, benchmark_records[$i].max_release_latency_cycles, benchmark_records[$i].max_activation_cycles, benchmark_records[$i].period_cycles, tasks[$i].flags, tasks[$i].stack_top - tasks[$i].stack_bottom, tasks[$i].high_water_words
  set $i = $i + 1
end
monitor halt
quit
'''

gdb = subprocess.Popen(
    [GDB, "-q"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
)
gdb.stdin.write(startup)
gdb.stdin.flush()
time.sleep(SECONDS)
gdb.stdin.write(report)
gdb.stdin.flush()
output, _ = gdb.communicate(timeout=10)
print(output)
```

For a scripted CCM integration, use the repository's application-side wrapper
around this same sequence. The important rules are to use a matching ELF, let
the target run freely, and send the report commands only after `interrupt` has
completed.
