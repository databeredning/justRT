# RTOS Roadmap

## Current milestone: concurrency hardening

Make blocking, wake-up, and timer operations race-free before expanding the
kernel API.

1. Atomic wait enrollment
   - Replace the split `inspect object -> leave critical section -> task_block()`
     sequence with an internal operation that checks the condition and enrolls
     the waiter while the same critical section is held.
   - Yield only after the blocked state has been committed.
   - Apply this consistently to semaphores, queues, mutexes, notifications, and
     event groups.

2. Deterministic wake-up semantics
   - Define whether waking transfers or reserves a semaphore, queue slot/item,
     or mutex ownership for the selected waiter.
   - Always select the highest-priority eligible waiter, with deterministic
     handling of equal priorities.
   - Maintain priority inheritance correctly when mutex waits time out or
     ownership changes.

3. Absolute timeout deadlines
   - Preserve one absolute deadline across retry loops so a spurious or
     intermediate wake-up cannot restart the full timeout.
   - Verify timeout comparisons across the 32-bit tick-counter wraparound.

4. Timer synchronization
   - Protect timer creation, list insertion, callback changes, start, stop, and
     restart against concurrent `kernel_timer_tick()` execution.
   - Define behavior when a timer is stopped or restarted concurrently with an
     expiry.
   - Keep callback execution out of SysTick context.

5. Adversarial regression test
   - Add a named `race` hardware test that repeatedly exercises:
     - semaphore give versus take enrollment
     - queue send/receive versus timeout
     - mutex unlock versus timeout
     - timer start/stop/restart versus expiry
     - tick-counter wraparound
   - Expose explicit pass, fail, done, and race-iteration counters to the
     debugger.
   - Add the test to `make auto-test` after it is stable.

6. Kernel invariants and diagnostics
   - Detect impossible states such as a blocked task without a wait object, an
     invalid mutex owner, duplicate timer-list links, or a READY task retaining
     wait metadata.
   - Record enough state for post-failure debugger inspection before stopping.

## Follow-on milestones

1. Add a kernel-owned timer service task so callbacks no longer require manual
   task-side dispatch.
2. Add per-task MPU data isolation.
3. Add a QEMU Cortex-M target for fast, repeatable CI regression testing.
4. Consider task suspend/resume and other lifecycle APIs only after ownership
   and cleanup rules are defined.

## Validation

For each focused change:

1. Build the affected named test with `make -B TEST=<name>`.
2. Run `git diff --check`.
3. Run the applicable hardware regression, and run `make auto-test` for shared
   scheduler, synchronization, or timer changes.
4. Inspect `g_fault_active`, `g_fault_record`, `g_context_switches`, and the
   test result fields in the debugger.
5. Keep generated `bin/` and `obj/` artifacts out of commits.
