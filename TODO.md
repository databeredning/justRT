# RTOS Roadmap

## Completed milestone: QEMU regression target

Add a fast Cortex-M target for repeatable scheduler and kernel regression
testing while retaining S32K312 hardware tests as the final acceptance gate.

1. Target and platform bring-up
   - [x] Add an isolated `qemu-mps2-an385` Cortex-M3 build target.
   - [x] Add minimal startup, vector table, linker script, and board layer.
   - [x] Keep QEMU build artifacts separate from S32K312 artifacts.
   - [x] Boot `simple` and confirm SysTick/PendSV switching without faults or
     invariant failures.

2. Automated regression
   - [x] Add an automated QEMU/GDB test runner.
   - [x] Run `boot`, `sync`, `mutex`, and `race` under QEMU.
   - [x] Report the same result, fault, and invariant diagnostics as the
     J-Link hardware runner.
   - [x] Add a convenient `make qemu-test` entry point.

3. Cross-target validation
   - [x] Run the complete S32K312 hardware suite after shared Cortex-M changes.
   - [x] Document supported QEMU tests and explicitly exclude the Cortex-M3
     `fpu` profile.

## Completed milestone: concurrency hardening

Blocking, wake-up, timeout, and timer operations are race-hardened before
further expansion of the kernel API.

1. Atomic wait enrollment
   - [x] Replace the split check-to-block sequence for semaphores.
   - [x] Replace the split check-to-block sequence for queue send and receive.
   - [x] Yield only after the blocked state has been committed.
   - [x] Apply atomic enrollment to mutexes.
   - [x] Confirm atomic enrollment for notifications and event groups.

2. Deterministic wake-up semantics
   - [x] Transfer semaphore tokens directly to selected waiters.
   - [x] Reserve queue slots and items for selected senders and receivers.
   - [x] Select the highest-priority eligible waiter, with task order breaking
     equal-priority ties deterministically.
   - [x] Transfer mutex ownership directly to the selected waiter.
   - [x] Maintain priority inheritance when mutex waits time out or ownership
     changes.

3. Absolute timeout deadlines
   - [x] Preserve one absolute deadline across retry loops so a spurious or
     intermediate wake-up cannot restart the full timeout.
   - [x] Verify timeout comparisons across the 32-bit tick-counter wraparound.

4. Timer synchronization
   - [x] Protect timer creation, list insertion, callback changes, start, stop, and
     restart against concurrent `kernel_timer_tick()` execution.
   - [x] Define behavior when a timer is stopped or restarted concurrently with an
     expiry.
   - [x] Keep callback execution out of SysTick context.

5. Adversarial regression test
   - [x] Add the named `race` hardware test and automated-runner profile.
   - [x] Exercise semaphore give versus take enrollment.
   - [x] Exercise empty-queue receive and full-queue send enrollment.
   - [x] Exercise mutex unlock versus timeout.
   - [x] Exercise timer start/stop/restart versus expiry.
   - [x] Exercise tick-counter wraparound.
   - [x] Expose explicit result and race diagnostics to the debugger.

6. Kernel invariants and diagnostics
   - [x] Detect impossible states such as a blocked task without a wait object, an
     invalid mutex owner, duplicate timer-list links, or a READY task retaining
     wait metadata.
   - [x] Record enough state for post-failure debugger inspection before stopping.

## Follow-on milestones

1. Add a kernel-owned timer service task so callbacks no longer require manual
   task-side dispatch.
2. Add per-task MPU data isolation.
3. Consider task suspend/resume and other lifecycle APIs only after ownership
   and cleanup rules are defined.

## Validation

For each focused change:

1. Build the affected named test with `make -B TEST=<name>`.
2. Run `git diff --check`.
3. Run the applicable hardware regression, and run `make auto-test` for shared
   scheduler, synchronization, or timer changes.
4. Run the applicable QEMU regression once the automated runner is available.
5. Inspect `g_fault_active`, `g_fault_record`, `g_context_switches`, and the
   test result fields in the debugger.
6. Keep generated `bin/` and `obj/` artifacts out of commits.
