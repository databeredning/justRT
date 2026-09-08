# Examples

`simple.c` is the single board-independent application example. It starts two
unprivileged tasks and continuously exercises first-task SVC startup, task scheduling,
`JRT_TaskYield()`, `JRT_TaskDelay()`, and the privileged idle task.

Build it with:

```sh
make -B TEST=simple
make -B TARGET=qemu-mps2-an385 TEST=simple
```

Inspect these debugger-visible values:

```text
g_simple_result.state == SIMPLE_STATE_RUNNING
g_simple_observer_runs > 0
g_simple_worker_runs > 0
```

The example performs no board or peripheral access. Kernel regression tests
are kept under `tests/` and are selected with `TEST=boot`, `TEST=sync`,
`TEST=mutex`, `TEST=fpu`, or `TEST=race`. The FPU profile is available only on
the S32K312 target.

## Benchmark Configuration

On S32K312, enable `JRT_ENABLE_TASK_BENCHMARK` and give periodic tasks an
explicit period when Budget reporting is useful:

```c
JRT_DECLARE_STATIC_TASK_STACK(periodic_stack, 128U);
JRT_DECLARE_STATIC_TASK_STACK(event_stack, 128U);

static const JRT_TaskDefinition_t benchmark_tasks[] = {
	JRT_TASK_DEFINITION_WITH_PERIOD(
		periodic_task, 0U, periodic_stack, 2U, "periodic", 0U, 10U),
	JRT_TASK_DEFINITION_WITH_PERIOD(
		event_task, 0U, event_stack, 1U, "event-driven", 0U, 0U)
};
```

The periodic task reports Budget against ten kernel ticks. The event-driven
task reports counts and timing with Budget shown as `N/A`. Use
`python tools/run_benchmark.py --flash --runtime 20` to produce the dynamic
report.

See [benchmark collection](../docs/BENCHMARK.md) for prerequisites and image paths.
