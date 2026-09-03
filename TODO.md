# RTOS Roadmap

## Completed milestones

<details>
<summary>Production-readiness hardening</summary>

### Production-readiness hardening

Turn the tested static kernel into a clearly bounded release configuration.
This milestone prioritizes explicit safety contracts, deterministic failure
handling, configuration validation, and reproducible validation over adding
dynamic task lifecycle features.

### Planned commit 1: define execution and interrupt safety contract

- [x] Classify public APIs by initialization, privileged task, unprivileged
  task, timer-callback, and ISR context.
- [x] Document the supported ISR gateways and rejection behavior for context
  misuse.
- [x] Document SysTick, PendSV, SVC, application interrupt, and PRIMASK
  critical-section behavior.
- [x] State callback blocking, nesting, latency, and NMI/fault-handler rules.

Suggested commit: `docs: define execution and interrupt safety contract`

### Planned commit 2: add configurable fatal-error hooks

- [x] Define application hooks for kernel invariants, stack overflow, and
  processor faults while retaining debugger-visible fault records.
- [x] Provide deterministic default stop behavior and an optional reset or
  watchdog handoff policy.
- [x] Ensure fatal paths mask interrupts, avoid scheduler re-entry, and cannot
  return accidentally.
- [x] Add focused custom-hook regression coverage on QEMU and S32K312.
- [x] Add regression coverage proving a returning hook reaches the kernel's
  default halt policy.

Suggested commit: `kernel: add configurable fatal-error hooks`

### Planned commit 3: validate release-time kernel settings

- [x] Extend compile-time checks for clocks, tick conversion, task limits,
  priorities, and all kernel-owned stack sizes.
- [x] Separate release diagnostics and test hooks from required production
  behavior without weakening invariant checks.
- [x] Reject unsupported or ambiguous configurations with actionable errors.
- [x] Add positive and negative configuration-build tests.

Suggested commit: `config: validate release-time kernel settings`

### Planned commit 4: add optimized and extended stress profiles

- [x] Run scheduler, synchronization, timer, suspension, and tick-wrap tests
  in an optimized release build.
- [x] Add longer deterministic stress profiles with bounded completion and
  reproducible seeds.
- [x] Record stack high-water, critical diagnostics, and binary size for
  debug and release configurations.
- [x] Keep expected MPU faults distinct from unexpected fatal diagnostics.

Suggested commit: `test: add optimized and extended stress profiles`

### Planned commit 5: automate QEMU release validation

- [x] Add CI that builds both targets and runs the complete QEMU suite.
- [x] Archive release ELF, map, binary, test results, and size reports.
- [x] Pin and report compiler, Python, QEMU, and debugger versions.
- [x] Make CI failure output preserve the existing debugger diagnostics.

Suggested commit: `ci: automate qemu release validation`

### Planned commit 6: publish the supported release configuration

- [x] Document the supported toolchain, targets, configuration envelope, and
  application integration checklist.
- [x] Document residual risks and explicitly unsupported contexts/features.
- [x] Run complete QEMU and S32K312 acceptance suites from a clean checkout.

Suggested commit: `docs: publish supported release configuration`

Acceptance: manual GitHub release validation passed; S32K312 hardware suites
passed with 17 debug profiles and 6 optimized release profiles.

Release: `v0.9.0-production-readiness`

</details>

<details>
<summary>Static task suspension</summary>

### Static task suspension

Allow statically configured application tasks to be suspended and resumed
without rebuilding their stack context or retaining a stale private-data MPU
mapping. The first version accepts `RUNNING` and `READY` application tasks and
deliberately rejects sleeping, blocked, internal, and invalid targets.

- [x] Define stable application task IDs and suspension lifecycle semantics.
- [x] Implement atomic self/other suspension and explicit resume.
- [x] Add validated SVC gateways for unprivileged tasks.
- [x] Verify saved stack/private state, scheduler exclusion, misuse handling,
  and private-memory revocation on QEMU and S32K312.
- [x] Document the state machine and validate the complete regression suites.

Release: `v0.8.0-task-suspension`

</details>

<details>
<summary>Per-task MPU data isolation</summary>

### Per-task MPU data isolation

Prevent one unprivileged application task from reading or writing another
task's private data while preserving explicitly shared kernel objects and the
existing stack-overflow protection.

- [x] Decouple application-task capacity from MPU stack-guard region count.
- [x] Define private, shared, and kernel-owned memory classes and ownership.
- [x] Validate private-region size, alignment, range, privilege, and overlap.
- [x] Dynamically replace the running task's stack guard and private-data MPU
  mappings before exception return.
- [x] Verify own and shared access plus cross-task read/write MemManage faults.
- [x] Validate the complete S32K312 and QEMU regression suites.

Release: `v0.7.0-mpu-isolation`

</details>

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

## Planned milestone: kernel task benchmarking

Implement task benchmarking as a standalone optional justRT kernel module so
every configured task is represented automatically and an external runner can
produce one consistent per-task timing grid. Keep the first version
deliberately small; it should answer whether a task keeps up with its releases
and how much of its configured period it consumes, without becoming a general
tracing system or requiring application-specific instrumentation.

Current status: the feature gate, task period metadata, and initial benchmark
record storage, cycle-source integration, and initial runtime accounting are
complete. Snapshot APIs, the deterministic S32K312 regression profile, and a
generic S32K312 report runner are implemented; broader regression coverage,
documentation, and final release validation remain in progress.

### First-version result

The kernel shall expose one stable snapshot entry for every configured
application task, plus the kernel timer-service task when enabled. Task names
and the number of rows shall come from the active `JRT_KernelConfig_t`; no
application-specific task table or fixed task-name list shall be required by
the benchmark runner.

The first report grid should contain only:

| Column | Kernel value or calculation | Purpose |
|---|---|---|
| Task | Configured task name | Identifies the row. |
| Releases | `release_count` | Number of benchmarked activations made ready. |
| Runs | `completion_count` | Number of activations that subsequently completed by blocking, sleeping, or suspending. |
| Pending | `release_count - completion_count - coalesced_count`, saturated at zero | Shows work released but not completed at snapshot time. |
| Coalesced | `coalesced_count` | Shows releases merged into an already-pending notification or activation. |
| Release max | `max_release_latency_cycles` converted to time | Worst delay from release to first execution. |
| Execution max | `max_activation_cycles` converted to time | Worst elapsed activation time from first execution until the task blocks again, including preemption. |
| Budget | `max_activation_cycles / period_cycles * 100` | Shows the worst observed use of the task's configured time slot. |
| Stack max | `used_stack_words / stack_words * 100` | Shows the maximum observed stack usage and remaining stack margin. Display used/configured words and percentage. |
| Result | Derived by the runner | `PASS` when there is no coalescing and maximum activation time is below the configured period; otherwise `CHECK`. |

Do not initially add averages, histograms, per-ISR attribution, dispatch phase,
callback duration, standard deviation, or CPU-load percentages. Those can be
added later only when a concrete diagnostic need exists.

### Measurement semantics

Define the semantics before adding counters so the same numbers remain useful
across applications:

1. A release occurs when a blocked or sleeping task becomes ready because of
   a notification, synchronization object, timeout, delay expiry, resume, or
   internal timer-service wake-up.
2. Release latency starts at that blocked-to-ready transition and ends the
   first time the released task is selected to run.
3. An activation starts at that first selection and completes when the task
   next blocks, sleeps, or is suspended. Ordinary preemption does not complete
   the activation.
4. Activation elapsed time includes time spent preempted. This is intentional:
   it measures consumption of the scheduling slot represented by
   `Execution max` and `Budget`. Pure CPU execution time is not part of the
   first version.
5. A notification added while the task already has an unconsumed notification
   is a coalesced release. Preserve the notification API's existing arithmetic
   and count the condition without changing scheduling behavior.
6. A task that is READY at kernel start has no timed release. Begin measuring
   it only after its first block and subsequent wake. Idle is excluded from
   the first report because it has no application period or activation model.
7. Period usage is available only for tasks with a nonzero benchmark period.
   A task without a configured period still reports counts and times, while
   its Budget and deadline Result are shown as `N/A`.
8. Stack max uses the kernel's existing stack-fill high-water measurement. It
   is the greatest observed stack usage, not the stack depth at snapshot time.
   Report both used/configured words and percentage; do not add a second stack
   scanner or duplicate the existing high-water accounting.

### Step 1: add a compile-time feature gate

- [x] Add `JRT_ENABLE_TASK_BENCHMARK` to `JRTConfig.h`, defaulting to `0`.
- [x] Compile all counters, timestamps, cycle-counter setup, and public
  benchmark APIs out when the feature is disabled.
- [x] Add configuration validation that accepts only `0` or `1`.
- [x] Reject benchmark enablement unless the selected target declares a
  supported cycle counter.
- [x] Confirm the disabled build has no task-structure growth, cycle reads, or
  scheduler hot-path branches after optimization.

Suggested commit: `config: add optional task benchmark feature`

### Step 2: configure each task's time slot

The kernel cannot derive an intended period from task priority, notification
traffic, or `JRT_TaskDelayUntil()` calls. Add one optional benchmark-only field
to `JRT_TaskDefinition_t`, expressed in kernel ticks:

```c
uint32_t benchmark_period_ticks;
```

- [x] Extend the task-definition macros so existing definitions default the
  field to zero and remain source-compatible.
- [x] Allow applications that want Budget reporting to specify the period
  explicitly, preferably through a named initializer or an additional task
  definition macro rather than positional initialization.
- [x] Convert the period to cycles when the kernel initializes benchmarking:
  `period_cycles = benchmark_period_ticks * core_clock_hz / JRT_TICK_RATE_HZ`.
- [x] Use 64-bit intermediate arithmetic and reject or mark unavailable any
  period that cannot be represented safely.
- [x] Give the internal timer-service task a zero period initially. Its timing
  may still be reported, but it has no single application scheduling slot.

Suggested commit: `kernel: configure optional task benchmark periods`

### Step 3: add a minimal kernel-owned record

Add a benchmark record indexed by scheduler task ID. Prefer a separate array
over enlarging `task_t`, because the feature can then disappear completely
from production builds:

```c
typedef struct
{
    uint32_t release_count;
    uint32_t completion_count;
    uint32_t coalesced_count;
    uint32_t release_cycle;
    uint32_t activation_start_cycle;
    uint32_t max_release_latency_cycles;
    uint32_t max_activation_cycles;
    uint32_t period_cycles;
    uint8_t release_pending;
    uint8_t activation_active;
} JRT_TaskBenchmarkRecord_t;
```

- [x] Allocate `JRT_MAX_SCHEDULER_TASKS` records statically in privileged
  kernel data; do not allocate memory dynamically.
- [x] Initialize only the active `task_count` entries during
  `JRT_KernelInit()` and clear all timestamps and maxima deterministically.
- [x] Keep task name, priority, and state in their existing owners. The public
  snapshot API can combine those values with the benchmark record instead of
  duplicating them.
- [x] Reuse `task_t.high_water_words` and the configured stack size for the
  basic stack benchmark. Do not add stack fields to the benchmark record or
  perform an additional stack scan.
- [x] Define all 32-bit counters as wrapping diagnostic counters. Timing
  differences must use unsigned subtraction so a single DWT wrap is handled.
- [x] State that one measured activation must be shorter than one full
  32-bit-cycle-counter wrap; reject benchmarking at initialization if no
  supported cycle source is available.

Suggested commit: `kernel: add per-task benchmark records`

### Step 4: provide a portable cycle-counter boundary

- [x] Add `arch_cycle_counter_init()`, `arch_cycle_counter_available()`, and
  `arch_cycle_counter_read()` to the port contract.
- [x] Implement the Cortex-M version with DWT `CYCCNT`, enabling trace and the
  counter once during kernel initialization rather than in application code.
- [x] Preserve any existing DWT counter values and unrelated control bits:
  initialization may only OR the required enable bits and must never clear,
  reset, or reload `DWT->CYCCNT`.
- [x] Record the cycle frequency used for conversion in benchmark metadata.
- [x] Keep raw values in cycles inside the kernel. Convert to microseconds,
  milliseconds, and percentages in the host runner to avoid floating-point
  work in the target.
- [x] Define QEMU behavior explicitly: the real benchmark profile is rejected
  for the unsupported Cortex-M3 target; QEMU timing support can be added later
  with a validated DWT or deterministic fake cycle source.

Suggested commit: `port: expose benchmark cycle counter`

### Step 5: centralize release accounting

Create small internal helpers called only while the kernel critical section is
held:

```c
task_benchmark_release_locked(task_id, now, coalesced);
task_benchmark_start_locked(task_id, now);
task_benchmark_complete_locked(task_id, now);
```

- [x] Call the release helper at every state transition that makes a task
  READY from BLOCKED, SLEEPING, or SUSPENDED. Centralize this in existing
  transition helpers such as `task_wait_end()` where possible so individual
  semaphore, queue, mutex, event, notification, and timeout paths cannot drift.
- [x] Instrument delay expiry in the SysTick task-state update path.
- [x] Instrument explicit task resume and the internal timer-service wake.
- [x] In `task_notify_common()`, count a coalesced release when notification
  data arrives while an earlier notification remains pending. Do not treat
  integer notification values as release counts unless that is already the
  documented API semantic.
- [x] Save `release_cycle` only for the oldest outstanding activation. A later
  coalesced release must not overwrite it and hide the true latency.
- [x] Saturate the derived Pending value at zero in snapshots so counter wrap
  or an in-progress transition cannot produce a misleading large value.

Suggested commit: `kernel: account task releases and coalescing`

### Step 6: measure activation start and completion

- [x] In the scheduler, when a released task is selected for its first run,
  calculate `now - release_cycle`, update
  `max_release_latency_cycles`, save `activation_start_cycle`, and mark the
  activation active.
- [x] Do not restart the activation timestamp when the same task resumes after
  ordinary preemption or round-robin scheduling.
- [x] Immediately before a running task changes to BLOCKED, SLEEPING, or
  SUSPENDED, calculate `now - activation_start_cycle`, update
  `max_activation_cycles`, increment `completion_count`, and clear the active
  flags.
- [x] Audit every task state assignment in `task.c` and route relevant
  transitions through common helpers. Document exclusions such as fatal stop
  and kernel shutdown.
- [x] Ensure timestamp reads and record updates occur inside the existing
  critical section and do not introduce an additional scheduler lock.
- [ ] Measure and record the added scheduler overhead in a dedicated test, but
  do not add that overhead measurement to the normal per-task grid.

Suggested commit: `scheduler: measure task activation latency and duration`

### Step 7: expose dynamic snapshot APIs

Add public read-only APIs rather than requiring applications or runners to
depend on private `task_t` layout:

```c
typedef struct
{
    uint32_t release_count;
    uint32_t completion_count;
    uint32_t pending_count;
    uint32_t coalesced_count;
    uint32_t max_release_latency_cycles;
    uint32_t max_activation_cycles;
    uint32_t period_cycles;
    uint32_t stack_words;
    uint32_t used_stack_words;
} JRT_TaskBenchmarkInfo_t;

typedef struct
{
    uint32_t enabled;
    uint32_t task_count;
    uint32_t cycle_frequency_hz;
} JRT_BenchmarkInfo_t;

JRT_Status_t JRT_BenchmarkGetInfo(JRT_BenchmarkInfo_t *info);
JRT_Status_t JRT_BenchmarkGetTask(uint32_t task_id, JRT_TaskBenchmarkInfo_t *info);
JRT_Status_t JRT_BenchmarkReset(void);
```

- [x] Make `task_count` dynamic and return entries by task ID. The runner shall
  iterate from zero to `task_count - 1` and obtain names with
  `JRT_TaskGetName()` or a combined snapshot API.
- [x] Copy a snapshot inside one short critical section. Never expose a
  writable pointer to kernel-owned records.
- [x] Populate `stack_words` and `used_stack_words` from the same task stack
  metadata used by `JRT_TaskGetStackInfo()` so the benchmark snapshot is
  internally consistent and requires only one task query per report row.
- [x] Define whether internal tasks are included. Recommended: include the
  timer-service task, exclude idle, and expose flags so the runner can label
  application versus internal rows.
- [x] Restrict reset to privileged task context. Reset counters and maxima
  atomically while preserving an active release/activation timestamp so the
  next completion cannot use a timestamp from before the reset.
- [x] Keep debugger-visible metadata and the record array in named globals if
  practical, allowing a halted debugger to inspect them even when the target
  application does not call the APIs.

Suggested commit: `api: expose dynamic task benchmark snapshots`

### Step 8: create one generic report runner

- [x] Extend the existing justRT GDB automation with a benchmark mode rather
  than adding an application-specific parser to the kernel repository.
- [x] Reset the benchmark, run for a configured duration, halt once, read
  benchmark metadata, iterate the reported task count, and resolve every task
  name dynamically.
- [x] Generate the compact grid defined above. Convert cycles using
  `cycle_frequency_hz`:
  `microseconds = cycles * 1000000 / cycle_frequency_hz` and
  `milliseconds = cycles * 1000 / cycle_frequency_hz`.
- [x] Calculate Budget only when `period_cycles != 0`:
  `budget_percent = max_activation_cycles * 100 / period_cycles`.
- [x] Calculate Stack max when `stack_words != 0`:
  `stack_percent = used_stack_words * 100 / stack_words`, using 64-bit
  intermediate arithmetic. Display `used_stack_words/stack_words` and the
  percentage so a small stack is not hidden by percentage rounding.
- [x] Mark `CHECK` for nonzero coalescing or Budget at or above 100%. Show a
  pending activation separately, because stopping between release and run is
  not automatically a failure.
- [x] Write both console output and a stable Markdown result. Keep raw cycle
  values accessible in verbose output for debugging and reproducibility.
- [x] Keep target details outside the benchmark parser. Supply ELF path,
  debugger server, device/interface, runtime, and output path through command
  line options or a small target configuration.

Suggested commit: `tools: report dynamic per-task benchmark results`

### Step 9: regression coverage

- [x] Add a deterministic profile with at least three application tasks using
  different periods and priorities.
- [x] Verify task enumeration, names, configured periods, and exclusion of
  unused static task slots.
- [ ] Verify release and completion counts for delay, notification,
  synchronization wake, timeout, resume, and timer-service wake paths.
- [ ] Force one notification coalescence and prove the oldest release
  timestamp is retained.
- [ ] Force known release latency and activation duration with a fake cycle
  counter, including unsigned subtraction across counter wrap.
- [ ] Halt with one task pending and prove Pending is reported without a false
  coalescing failure.
- [ ] Verify reset during idle, pending release, and active activation.
- [ ] Exercise known stack depths and verify Stack max is monotonic, agrees
  with `JRT_TaskGetStackInfo()`, and never exceeds the configured stack size.
- [ ] Verify benchmark reset clears timing/counter maxima but does not erase
  the kernel's lifetime stack high-water value.
- [ ] Run disabled-build size and scheduler-overhead comparisons.
- [ ] Run the QEMU suite and then the complete S32K312 hardware suite, checking
  the generated dynamic report against the deterministic task-profile
  expectations.

Suggested commit: `test: validate kernel task benchmarking`

### Step 10: standalone integration and documentation

- [x] Keep the module self-contained behind a benchmark header and internal
  implementation boundary. Application code shall need only the optional task
  period configuration; it shall not call start/stop timing hooks around task
  bodies.
- [ ] Provide a minimal example configuration with periodic, event-driven, and
  internal tasks to demonstrate dynamic enumeration and `N/A` Budget handling.
- [x] Document the kernel-defined activation boundaries and remove any metric
  whose semantics cannot be made stable across supported task wait types.
- [x] Document configuration, timing semantics, overhead, counter-wrap limit,
  debugger usage, snapshot API usage, and interpretation of Pending,
  Coalesced, Execution max, Budget, and Stack max.
- [x] Publish the generic runner independently of any product build system;
  board repositories may wrap it in their own make targets without changing
  the justRT benchmark module or report parser.

Acceptance criteria:

1. Adding or removing an application task changes the report automatically;
   no runner source change is needed.
2. Every configured application task appears by name, and unused task slots do
   not appear.
3. The grid identifies missed/coalesced activations and shows maximum slot and
   stack usage with no application-side timing calls.
4. Feature-disabled production builds have no benchmark storage or scheduler
   overhead.
5. QEMU and S32K312 regression suites pass, and repeated fixed-duration runs
   produce counts and timing bounds consistent with the deterministic test
   profiles.

Suggested release: `v0.10.0-task-benchmarking`

## Optional later milestone

Consider task deletion or static-slot reactivation only when an application
requires it; neither is required for a production-ready static kernel.
Deletion must define mutex-owner handling, wait-list removal, timeout
cancellation, stale task IDs, private-memory clearing, and MPU revocation
without introducing heap allocation implicitly.

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
