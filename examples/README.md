# Example

`simple.c` is the single board-independent application example. It starts two
unprivileged tasks and continuously exercises first-task SVC startup, task scheduling,
`JRT_TaskYield()`, `JRT_TaskDelay()`, and the privileged idle task.

Build it with:

```sh
make -B TEST=simple
```

Inspect these debugger-visible values:

```text
g_simple_result.state == SIMPLE_STATE_RUNNING
g_simple_observer_runs > 0
g_simple_worker_runs > 0
```

The example performs no board or peripheral access. Kernel regression tests
are kept under `tests/` and are selected with `TEST=boot`, `TEST=sync`, or
`TEST=mutex`.
