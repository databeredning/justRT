# RTOS Roadmap

## Current milestone

- [x] S32K312 Cortex-M7 startup and scheduler
- [x] Semaphores, mutexes, queues, timeouts, and priority inheritance
- [x] ISR-safe synchronization paths and diagnostics
- [x] MPU stack guards and privileged/unprivileged task execution
- [x] SVC yield and sleep services
- [x] Privileged LED gateway for unprivileged tasks
- [x] Retire legacy board-dependent examples and numeric profiles
- [x] Shared wait/wake helpers (`task_wait_begin`/`task_wait_end`/`task_wait_reset`)
- [x] Named boot, synchronization, and mutex regression tests
- [x] Full architecture extraction: portable kernel (`kernel/task.c`,
      `sync.c`, `timer.c`, `mempool.c`) contains zero Cortex-M register
      access; `arch/cortex_m/port_contract.h` documents the seam
      (`arch_request_switch`, `arch_critical_enter`/`arch_critical_exit`,
      `arch_in_isr`, `arch_tick_init`, `arch_yield`, `arch_configure_mpu`,
      `arch_start_first_task`, `arch_wait_for_interrupt`)
- [x] Platform layer extracted to `platform/s32k312/` (startup, vector
      table, system init, linker script, board)
- [x] Documentation reorganized: `RTOS.md` (kernel/API), `examples/README.md`
      (application examples), `PORT.md` (new-port guide), all validated
      against named hardware tests

## Next features

### Nitpicks

- Keep the first-task startup SVC first in the service enum so its bootstrap
      role remains obvious.

1. Task lifecycle
   - Suspend and resume
   - Periodic runtime statistics
   - Restart and delete only after ownership rules are defined

2. Second port to prove the architecture split
   - Follow `PORT.md`; QEMU `mps2-an385` (Cortex-M3) is the suggested target
   - Confirms `arch/cortex_m` is genuinely reusable and not S32K312-shaped
     by accident

## Working method

Each roadmap item should be an isolated, buildable commit:

1. Read the owning implementation and nearest regression surface.
2. State one local behavior hypothesis and one cheap check that can falsify it.
3. Make the smallest focused edit.
4. Run the narrowest available build or regression check immediately.
5. Build the exact named test with `make -B TEST=<name>`.
6. Flash and inspect debugger-visible pass counters and fault state.
7. Commit only after hardware behavior is confirmed.
8. Keep the default `TEST=simple` example stable unless the item explicitly
      changes it.
9. Add or update documentation for user-visible behavior and diagnostics.
10. Tag only validated milestones, using a meaningful annotated tag.

Useful checks:

```bash
make -B TEST=simple
git diff --check
arm-none-eabi-readelf -SW bin/justrt.elf
arm-none-eabi-nm -n bin/justrt.elf
git status --short --branch
```

Keep experimental work in a separate commit or temporary branch. Do not mix
unrelated cleanup, linker changes, and runtime behavior changes in one commit.
