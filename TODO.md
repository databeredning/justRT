# RTOS Roadmap

## Current milestone: kernel timer service task

Dispatch software-timer callbacks automatically from a kernel-owned task so
applications no longer need to poll expirations or call
`JRT_TimerDispatch()` manually.

1. Service-task ownership and scheduling
   - [x] Centralize application-selectable kernel settings in `JRTConfig.h`
     while keeping kernel-owned task capacity private.
   - [x] Separate the public application-task limit from internal scheduler
     task-table capacity.
   - [x] Add a statically allocated kernel timer-service task and stack.
   - [x] Reserve its task-table and stack-guard capacity without reducing the
     documented application-task limit unexpectedly.
   - [x] Define its priority relative to application and idle tasks.

2. Expiry notification and callback dispatch
   - [x] Keep SysTick limited to recording expirations and waking the service
     task; never execute callbacks in exception context.
   - [x] Drain pending callbacks without holding the kernel critical section
     while application callback code runs.
   - [x] Preserve accumulated expirations and the existing stop/start/restart
     race semantics.
   - [x] Define behavior when callbacks start, stop, restart, or reconfigure
     their own timer or another timer.

3. API transition
   - [ ] Keep expiration polling available for timers without callbacks.
   - [x] Decide whether `JRT_TimerDispatch()` remains as a compatibility API
     or becomes kernel-internal.
   - [x] Document callback execution context, ordering, and blocking rules.

### Timer-service execution contract

- The kernel-owned timer-service task has priority 1, immediately above the
  priority-0 idle task. Application tasks with priority greater than 1 preempt
  timer callbacks; priority-1 application tasks share the processor with the
  service task under the scheduler's normal round-robin rule.
- Callbacks execute serially in privileged Thread mode on the timer-service
  task's stack. They never execute in SysTick or while the kernel critical
  section is held.
- Callbacks must not block, delay, or wait for synchronization. A callback
  should perform bounded work or notify an application task that owns the
  longer-running operation. One callback that does not return prevents every
  other timer callback from being dispatched.
- Each recorded expiration with a non-null callback produces one callback
  invocation. Delayed periodic expirations accumulate and are not collapsed.
  Pending timers are scanned in timer-list order; no ordering guarantee is
  provided between different timers that expire on the same tick.
- The service task claims one invocation atomically by capturing the callback
  and argument and consuming one pending expiration. It releases the critical
  section before invoking the callback. Callback or argument changes affect
  only invocations that have not yet been claimed.
- Stopping a timer prevents future expirations but does not discard recorded
  expirations or revoke an invocation already claimed by the service task.
  Starting or restarting a timer sets its next deadline without discarding
  recorded expirations. These rules also apply when a callback operates on
  itself or another timer.
- Timers without callbacks retain their expiration counts for
  `JRT_TimerTakeExpirations()`. `JRT_TimerDispatch()` remains public during the
  transition for source compatibility, although new application code should
  rely on automatic service-task dispatch for timers with callbacks.

4. Regression coverage
   - [ ] Add a terminating timer-service regression profile.
   - [ ] Cover one-shot and periodic callbacks, multiple pending expirations,
     callback reconfiguration, and concurrent stop/restart operations.
   - [ ] Verify callback execution is task context and does not occur while a
     kernel critical section is held.
   - [ ] Add the profile to QEMU and S32K312 automated runners.

5. Cross-target validation
   - [ ] Run `make qemu-test` with the timer-service profile enabled.
   - [ ] Run the complete S32K312 `make auto-test` hardware suite.
   - [ ] Confirm no fault, invariant, stack, or lost-expiration diagnostics.

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

1. Add per-task MPU data isolation.
2. Consider task suspend/resume and other lifecycle APIs only after ownership
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
