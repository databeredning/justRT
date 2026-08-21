# RTOS Roadmap

## Current milestone

- [x] S32K312 Cortex-M7 startup and scheduler
- [x] Semaphores, mutexes, queues, timeouts, and priority inheritance
- [x] ISR-safe synchronization paths and diagnostics
- [x] MPU stack guards and privileged/unprivileged task execution
- [x] SVC yield and sleep services
- [x] Privileged LED gateway for unprivileged tasks
- [ ] Validate the time-foundation checkpoint on hardware

## Next features

1. Tick and timeout abstraction
   - Monotonic tick counter
   - Wrap-safe deadline comparisons
   - `task_delay_until()` for drift-free periodic work
   - Convert timeout paths to shared helpers

2. Software timers
   - One-shot timers
   - Periodic timers
   - Start, stop, restart, and expiry state
   - Timer service task for callbacks
   - Never execute arbitrary callbacks inside SysTick

3. Task notifications
   - Direct task signal
   - Counter and bit notifications
   - Blocking wait with timeout
   - ISR notification path

4. Event groups
   - Set and clear bits
   - Wait-any and wait-all
   - Optional clear-on-exit behavior

5. Deterministic memory pools
   - Fixed-size block allocation
   - Exhaustion behavior
   - Double-free and ownership diagnostics

6. SVC and MPU hardening
   - Validate service numbers and caller context
   - Validate pointer-bearing arguments before adding such services
   - Preserve invalid-service diagnostics
   - Add controlled gateway tests

7. Task lifecycle
   - Suspend and resume
   - Periodic runtime statistics
   - Restart and delete only after ownership rules are defined

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
