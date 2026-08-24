# RTOS Roadmap

## Current milestone

- [x] S32K312 Cortex-M7 startup and scheduler
- [x] Semaphores, mutexes, queues, timeouts, and priority inheritance
- [x] ISR-safe synchronization paths and diagnostics
- [x] MPU stack guards and privileged/unprivileged task execution
- [x] SVC yield and sleep services
- [x] Privileged LED gateway for unprivileged tasks
- [x] Validate the time-foundation checkpoint on hardware (profile 7)
- [x] Validate timer expiry bookkeeping on hardware (profile 8)
- [x] Validate deferred timer callbacks on hardware (profile 9)
- [x] Validate task notifications on hardware (profile 10)
- [x] Validate event groups on hardware (profile 11)
- [x] Validate deterministic memory pools on hardware (profile 12)
- [x] Shared wait/wake helpers (`task_wait_begin`/`task_wait_end`/`task_wait_reset`)
- [x] Event-group timeout/ISR regression (profile 13)
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
      against hardware profiles 0-13

## Next features

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
5. Build the exact hardware profile with `make -B MAIN_PROFILE=<number>`.
6. Flash and inspect debugger-visible pass counters and fault state.
7. Commit only after hardware behavior is confirmed.
8. Keep the default profile stable unless the item explicitly changes it.
9. Add or update documentation for user-visible behavior and diagnostics.
10. Tag only validated milestones, using a meaningful annotated tag.

Useful checks:

```bash
make -B MAIN_PROFILE=0
git diff --check
arm-none-eabi-readelf -SW bin/justboot.elf
arm-none-eabi-nm -n bin/justboot.elf
git status --short --branch
```

Keep experimental work in a separate commit or temporary branch. Do not mix
unrelated cleanup, linker changes, and runtime behavior changes in one commit.
