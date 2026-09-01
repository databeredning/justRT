# RTOS Roadmap

## Current milestone: per-task MPU data isolation

Prevent one unprivileged application task from reading or writing another
task's private data while preserving explicitly shared kernel objects and the
existing stack-overflow protection.

### Planned commit 1: decouple task capacity from MPU guard count

- [x] Replace the current one-per-task MPU stack-guard allocation with a
  dynamic guard for the currently running task.
- [x] Program the first task's guard before entering Thread mode and replace it
  during every context switch before exception return.
- [x] Decouple scheduler table capacity and `JRT_MAX_APPLICATION_TASKS` from
  the hardware MPU region count.
- [x] Keep `JRT_MAX_APPLICATION_TASKS` configurable and choose a documented
  default from static RAM cost and worst-case scheduler scan time rather than
  MPU region count.
- [x] Add boundary coverage for the configured maximum and maximum-plus-one
  task counts.
- [x] Verify dynamic stack guards still capture overflow on S32K312 and that
  the complete QEMU and hardware suites remain green.

Suggested commit: `arch: decouple task capacity from MPU guards`

### Planned commit 2: define private-memory configuration

- [x] Define private, shared, and kernel-owned memory classes and their access
  rules.
- [x] Define task ownership and the treatment of intentionally shared kernel
  objects and buffers.
- [x] Extend the static task-definition API without breaking tasks that do not
  request a private data region.
- [x] Store validated private-region base and size metadata in each task
  control block.
- [x] Enforce alignment, size, power-of-two, overlap, and address-range rules
  during `JRT_KernelInit()`.
- [x] Add QEMU-compatible configuration-validation tests for valid and invalid
  private-region definitions.

Suggested commit: `api: add task-private memory configuration`

### Planned commit 3: enforce per-task MPU data isolation

- [x] Reserve an MPU region for the running task's private data without
  weakening kernel, flash, peripheral, shared-RAM, or stack-guard protection.
- [x] Program the first task's private region before entering unprivileged
  Thread mode.
- [x] Replace the outgoing private region with the incoming task's region on
  every context switch before exception return.
- [x] Disable the dynamic private region for tasks that do not declare one and
  keep privileged kernel tasks independent of application-private mappings.
- [x] Preserve explicit ownership rules so future suspend/resume or deletion
  APIs cannot leave stale MPU access.

Suggested commit: `arch: enforce task-private MPU regions`

### Planned commit 4: add isolation and fault regressions

- [ ] Add a terminating MPU-isolation hardware profile.
- [ ] Verify a task can access its own private data and explicitly shared
  objects.
- [ ] Verify cross-task private reads and writes produce a captured MemManage
  fault with useful address, task, and exception diagnostics.
- [ ] Verify context switches revoke the outgoing task's private access.
- [ ] Add the profile and its diagnostics to the S32K312 automated runner.

Suggested commit: `test: add MPU isolation regression coverage`

### Planned commit 5: document and validate MPU isolation

- [ ] Document task-capacity policy, private/shared memory behavior, linker and
  section placement, and Cortex-M MPU granularity limits.
- [ ] Run the focused S32K312 stack-guard and MPU-isolation tests.
- [ ] Run the complete S32K312 `make auto-test` hardware suite.
- [ ] Run `make qemu-test` and confirm configuration and shared scheduler
  behavior are unchanged.
- [ ] Confirm no unexpected fault, invariant, stack, access-revocation, or
  scheduler diagnostics.

Suggested commit: `docs: document and validate MPU isolation`

## Completed milestones

<details>
<summary>Kernel timer service task</summary>

### Kernel timer service task

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
   - [x] Keep expiration polling available for timers without callbacks.
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
   - [x] Add a terminating timer-service regression profile.
   - [x] Cover one-shot and periodic callbacks, multiple pending expirations,
     callback reconfiguration, and concurrent stop/restart operations.
   - [x] Verify callback execution is task context and does not occur while a
     kernel critical section is held.
   - [x] Add the profile to QEMU and S32K312 automated runners.

5. Cross-target validation
   - [x] Run `make qemu-test` with the timer-service profile enabled.
   - [x] Run the complete S32K312 `make auto-test` hardware suite.
   - [x] Confirm no fault, invariant, stack, or lost-expiration diagnostics.

</details>

<details>
<summary>QEMU regression target</summary>

### QEMU regression target

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

</details>

<details>
<summary>Concurrency hardening</summary>

### Concurrency hardening

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

</details>

## Later milestone

Consider task suspend/resume and other lifecycle APIs only after ownership and
cleanup rules are defined by the MPU-isolation milestone.

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
