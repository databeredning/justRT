# justRT

## Scope

justRT is a statically configured preemptive RTOS for Cortex-M targets. It
currently supports the NXP S32K312 Cortex-M7 and QEMU's MPS2-AN385 Cortex-M3.
It uses no heap or C runtime. Tasks use PSP, reset and exception handlers use
MSP, SysTick provides timekeeping, and PendSV performs context switching.

## Source Layout

- `kernel/`: portable scheduler, synchronization, timers, and memory pools.
- `arch/cortex_m/`: architecture contract, initial task context, and CPU fault handling.
- `arch/cortex_m/port_cm7.c`: shared Cortex-M port, optional MPU, SVC dispatch,
  SysTick, and PendSV. The historical filename is retained.
- `arch/cortex_m/svc_cm7.s`: SVC exception handler and context restore helper.
- `arch/cortex_m/svc_stubs_cm7.c`: unprivileged SVC wrappers.
- `platform/s32k312/`: startup, vectors, linker script, and board driver.
- `platform/qemu_mps2_an385/`: QEMU startup, vectors, linker script, and board
  driver.
- `examples/`: board-independent examples.
- `tests/`: named regression firmware profiles.

## Task Model

Tasks are supplied statically through `JRT_KernelConfig_t`:

```c
JRT_DECLARE_STATIC_TASK_STACK(worker_stack, 256U);

static const JRT_TaskDefinition_t tasks[] = {
    JRT_TASK_DEFINITION(entry, argument, worker_stack, priority, "name", flags)
};
```

Tasks that need an isolated writable object declare an MPU-compatible region
and attach it to the task definition:

```c
typedef struct { uint32_t words[8]; } worker_private_t;
static worker_private_t worker_private JRT_TASK_PRIVATE_DATA(32U);

static const JRT_TaskDefinition_t tasks[] = {
    JRT_TASK_DEFINITION_WITH_PRIVATE_DATA(
        entry, &worker_private, worker_stack, priority, "worker",
        JRT_TASK_FLAG_UNPRIVILEGED, worker_private)
};
```

Writable memory has three ownership classes. Kernel-owned objects remain in
privileged data and are accessed only by privileged kernel code. Explicitly
shared application objects use `JRT_TASK_UNPRIVILEGED_DATA` and are readable
and writable by every unprivileged task. Task-private objects use
`JRT_TASK_PRIVATE_DATA()` and may be owned by exactly one unprivileged task.
The private size must be at least 32 bytes and a power of two, and its base
must be aligned to that size. Its range must remain inside
`.task_private_data` and must not overlap a stack, internal kernel stack, or
another private region.
`JRT_TASK_DEFINITION()` remains valid and requests no private region. This
metadata is validated during `JRT_KernelInit()`. On MPU-enabled targets, only
the running task's private region is accessible to unprivileged Thread mode.
Put all private state needed by one task into a single aggregate whose total
size is an MPU-compatible power of two. Do not place intentionally shared
queues, synchronization objects, or exchange buffers in that aggregate; place
those in `JRT_TASK_UNPRIVILEGED_DATA` and treat them as accessible to every
unprivileged task.

The public configuration supports up to `JRT_MAX_APPLICATION_TASKS` (seven)
application tasks. Scheduler storage privately reserves three additional
slots for kernel-owned tasks. The idle and timer-service tasks are created
internally. The timer-service task blocks while no callback work is pending.
The default of seven application tasks is a conservative static-RAM and
linear scheduler-scan policy, not an MPU limit. Applications may override
`JRT_MAX_APPLICATION_TASKS`; the scheduler table grows with that setting and
kernel initialization still rejects configurations above the selected limit.
In the current 32-bit build each scheduler slot costs 104 bytes before the
separately supplied task stack. Seven application tasks plus the two active
internal tasks require at most nine entries in each scheduler selection pass;
the table retains one additional reserved kernel slot. Selection is linear,
with one priority-discovery pass and at most one full tie-breaking pass.
States are `READY`, `RUNNING`, `SLEEPING`, `BLOCKED`, and `SUSPENDED`. Higher
numeric priorities run first; equal priorities are selected round-robin.

Application task IDs are the stable zero-based positions in the task array
passed to the successful `JRT_KernelInit()` call. They remain valid for that
static kernel configuration. Internal idle and timer-service indices are not
application task IDs and lifecycle APIs reject them. `JRT_TASK_ID_SELF` may be
passed to `JRT_TaskSuspend()` to identify the calling task without hard-coding
its configured index.

The suspension API contract is intentionally narrow. `JRT_TaskSuspend()` and
`JRT_TaskResume()` are task-context operations; ISR calls return
`JRT_STATUS_INVALID_CONTEXT`; pre-scheduler Thread mode also has no valid task
context. Suspend accepts the calling `RUNNING` task or another `READY`
application task. Sleeping, blocked, or already suspended targets return
`JRT_STATUS_INVALID_STATE`. Resume accepts only a suspended application task
and makes it ready; it does not restore or invent a previous wait. Invalid and
kernel-owned IDs return `JRT_STATUS_INVALID_TASK`. Suspension requires an
explicit resume and is not ended by ticks, notifications, queues, semaphores,
or events. Unprivileged calls use SVC 4 and 5; the privileged handlers validate
the target and pend normal scheduler selection before exception return.

The supported lifecycle transitions are:

| Operation | Source | Destination | Notes |
|---|---|---|---|
| Scheduler selects task | `READY` | `RUNNING` | Highest priority, round-robin among equals |
| Yield or preemption | `RUNNING` | `READY` | Saved execution context is retained |
| Finite delay | `RUNNING` | `SLEEPING` | Tick expiry returns the task to `READY` |
| Synchronization wait | `RUNNING` | `BLOCKED` | Signal or timeout returns the task to `READY` |
| Suspend self | `RUNNING` | `SUSPENDED` | The call returns only after another task resumes it |
| Suspend another task | `READY` | `SUSPENDED` | The target receives no CPU time while suspended |
| Resume | `SUSPENDED` | `READY` | Original priority, stack, notification value, and private-data ownership are retained |

Suspension does not cancel a delay or synchronization wait, so `SLEEPING` and
`BLOCKED` tasks must first become `READY` through their normal wake-up path.
Similarly, resume accepts neither `JRT_TASK_ID_SELF` nor a task that is already
ready or running. When a suspended task is not selected, its private MPU region
is not mapped. After resume, the region is restored only when the scheduler
selects that task again.

Each task supplies a statically allocated stack whose size is selected by the
application. `JRT_DEFAULT_TASK_STACK_WORDS` is 128 words for applications that
do not need a custom size. `JRT_DECLARE_STATIC_TASK_STACK()` places an aligned
32-byte MPU guard immediately below the stack and
`JRT_TASK_DEFINITION()` registers both with the kernel. The kernel owns the
idle-task stack and still performs no heap allocation.

Application-owned settings live in `JRTConfig.h`; the bundled header supplies
example/test defaults. See [application configuration](../README.md#application-configuration)
for selecting a custom header consistently across the build. These settings
select the core clock, tick rate, application-task limit, default application
stack size, idle and timer-service stack sizes, maximum task priority,
timer-service priority, and test-hook inclusion. Kernel-owned task capacity
and derived architecture limits remain private implementation details.

`JRT_ENABLE_TEST_HOOKS` defaults to zero. Production builds therefore omit the
optional SysTick hook load/call path while retaining fault records, stack
diagnostics, and kernel-invariant checks. Regression profiles that require
controlled tick-time injection enable the hook explicitly.

Kernel initialization rejects null, undersized, oddly sized, misaligned,
non-adjacent, overflowing, or overlapping stack/guard ranges. Stack sizes are
specified in 32-bit words and must be even so the initial exception frame has
the required 8-byte alignment.

## Task Benchmarking

Task benchmarking is an optional S32K312 diagnostic feature enabled with
`JRT_ENABLE_TASK_BENCHMARK=1`. The target must declare
`JRT_ARCH_HAS_DWT_CYCCNT=1`; the QEMU Cortex-M3 target is rejected because its
DWT cycle-counter behavior is not part of the supported timing contract.

Applications opt into period-based Budget reporting with
`JRT_TASK_DEFINITION_WITH_PERIOD()` or
`JRT_TASK_DEFINITION_WITH_PRIVATE_DATA_AND_PERIOD()`. Periods are expressed in
kernel ticks. A zero period is valid and reports timing without Budget.

The kernel records releases, notification coalescing, first-run latency,
activation duration including preemption, and existing stack high-water usage.
Records are exposed through `JRT_BenchmarkGetInfo()` and
`JRT_BenchmarkGetTask()`. `JRT_BenchmarkReset()` is privileged task-context
only and rebases active timestamps so post-reset measurements exclude time
before the reset. Counters wrap at 32 bits; an activation must complete within
one cycle-counter wrap.

The benchmark initializer enables DWT tracing and cycle counting without
clearing or reloading `DWT->CYCCNT` or changing unrelated DWT control bits.
The generic host report is generated with
`python tools/run_benchmark.py --runtime 20` and converts raw
cycles to time on the host.

The initial frame contains a saved `EXC_RETURN`, eight software-saved
registers, and the standard eight-word Cortex-M hardware frame:

```text
EXC_RETURN | r4-r11 | r0 r1 r2 r3 r12 LR PC xPSR
```

The saved task-context pointer is 4-byte aligned because `EXC_RETURN` adds one
word ahead of `r4-r11`. After the software context is restored, the resulting
hardware PSP is again 8-byte aligned as required by the exception-return ABI.

With `JRT_ARCH_FPU_CONTEXT` enabled, the architectural minimum is 51 words:
17 for the basic context, 16 for `s16-s31`, and 18 for the hardware FP frame.
Normal C call depth, local variables, and interrupt headroom require additional
space beyond this minimum.

The task argument is restored in `r0`. A returning task enters
`task_exit_trap()`.

## Startup and Context Switching

`JRT_KernelStart()` configures SysTick and executes startup SVC 0. The SVC
handler starts the tick while SVC still masks the lower-priority SysTick and
PendSV exceptions, then restores the first task. This prevents a context
switch before PSP has been initialized. The SVC handler:

Before issuing SVC 0, the port normalizes `CONTROL` to privileged Thread mode
using MSP with FPCA clear. This prevents PSP or floating-point state used by
pre-scheduler application code from changing the bootstrap exception frame.

1. Loads the current task's saved stack pointer.
2. Restores the task's saved `EXC_RETURN` and `r4-r11` with
   `arch_restore_task_context()`.
3. Sets PSP and the task's CONTROL value.
4. Returns with PSP `EXC_RETURN`.

The processor restores the hardware frame and enters the task. Later PendSV
saves the outgoing `EXC_RETURN` and `r4-r11`, calls `pendsv_switch()`, restores
the selected task through the same helper, updates CONTROL, and returns through
its saved exception return value.

When `JRT_ARCH_FPU_CONTEXT` is enabled, PendSV also saves and restores
`s16-s31` when `EXC_RETURN` bit 4 indicates that the task owns an extended
floating-point exception frame. Tasks that have not used floating point keep
the basic frame and do not incur this additional context cost.

Before the scheduler starts, the Cortex-M port enables CP10/CP11 and automatic
lazy floating-point stacking (`FPCCR.ASPEN` and `FPCCR.LSPEN`). During task
restore, `CONTROL.FPCA` is set only when the selected task's saved
`EXC_RETURN` identifies an extended floating-point frame.

The stack split is:

```text
MSP: reset, kernel code, SVC, SysTick, PendSV, and fault handlers
PSP: task code and task stacks
```

## SVC ABI

SVC wrappers are placed in `.unprivileged_svc`; exception handling remains
privileged.

| Number | Service |
|---:|---|
| 0 | Start first task; MSP caller only |
| 1 | Yield and request PendSV |
| 2 | Sleep for the tick count in `r0` |
| 3 | Toggle the board LED through privileged code |
| 4 | Suspend the application task identified by `r0`; return status in `r0` |
| 5 | Resume the application task identified by `r0`; return status in `r0` |

Normal task SVC calls require Thread mode using PSP. The SVC handler reads the
number from the instruction before the stacked PC and rejects invalid context
or service numbers. MSP/PSP points at the core-register frame for both basic
and extended floating-point exception frames; the additional hardware FP
registers occupy the higher-address portion of an extended frame.

## MPU

The S32K312 target enables the MPU. `arch_configure_mpu()` clears all region
slots and installs the following static and dynamic map:

| Region | Contents | Access |
|---:|---|---|
| 0 | Base internal flash | Privileged read/execute only |
| 1 | Base internal SRAM | Privileged read/write, XN |
| 2 | `.unprivileged_functions` | Read/execute both privilege levels |
| 3 | `.unprivileged_svc` | Read/execute both privilege levels |
| 4 | `.unprivileged_rodata` | Read-only, XN |
| 5 | `.unprivileged_task_data` | Read/write, XN |
| 14 | Running task's dynamic private data | Read/write, XN |
| 15 | Running task's dynamic stack guard | No access, XN |

Higher region numbers override lower ones. The linker aligns the explicit
unprivileged sections to MPU-compatible boundaries. `PRIVDEFENA` remains set
for the privileged background map. Task privilege is selected by
`JRT_TASK_FLAG_UNPRIVILEGED` and reapplied by PendSV. Region 15 is installed
for the first task during MPU initialization and replaced with the selected
task's 32-byte guard before every exception return. This makes task capacity
independent of MPU region count. The QEMU Cortex-M3 target builds with
`JRT_ARCH_HAS_MPU=0`; its stack bounds are checked in software and the dynamic
guard transition is exposed diagnostically, but QEMU does not validate MPU
enforcement.

`.task_private_data` is deliberately absent from the static unprivileged map.
Region 14 exposes only the selected task's validated private region and is
replaced before each exception return. It is disabled for tasks without
private data, including the kernel-owned idle and timer-service tasks. Any
future task suspend, resume, or deletion API must preserve this rule and must
not leave a stale private mapping installed.

The Cortex-M MPU represents a region with a power-of-two size and a base
aligned to that size; 32 bytes is the architectural minimum used here. A
33-byte private aggregate therefore needs a 64-byte enclosing object aligned
to 64 bytes. The kernel does not round or widen application declarations,
because doing so could unintentionally grant access to adjacent data. It
rejects configurations that cannot be represented exactly instead.

## Kernel Services

- Binary semaphores with task and ISR give operations.
- Recursive mutexes with priority inheritance and chain restoration.
- Bounded queues with task send/receive and non-blocking ISR send.
- Per-task accumulated notifications.
- Event groups with wait-any, wait-all, and clear-on-exit options.
- One-shot and periodic software timers. SysTick records expirations and wakes
  the kernel timer-service task, which claims each pending invocation under the
  kernel critical section and executes its callback after leaving the critical
  section. Timer configuration and expiry processing are serialized. If expiry
  wins a race with stop, start, or restart, that expiration remains pending
  while the later operation controls the timer's next deadline.
- Fixed-size memory pools protected by critical sections.

Task waits convert relative tick timeouts to one absolute deadline when the API
is entered. The deadline is retained across internal retry loops. Expiry uses
unsigned elapsed-tick arithmetic, which remains valid across 32-bit tick
wraparound for every finite timeout value.
`JRT_WAIT_FOREVER` selects an infinite wait. Synchronization APIs intended for
tasks reject ISR use, while dedicated ISR APIs are non-blocking.

## Execution Context and Interrupt Safety

justRT distinguishes privileged initialization code, privileged application
tasks, unprivileged application tasks, the kernel timer-service task, and
exception context. An API is supported only in the contexts listed below;
successful execution in another context is not part of the contract.

| API group | Initialization | Privileged task | Unprivileged task | Timer callback | Maskable ISR |
|---|---:|---:|---:|---:|---:|
| `JRT_KernelInit()`, static object creation, `JRT_KernelStart()` | Yes | No | No | No | No |
| Yield, delay, suspend, resume, and LED SVC | No | Yes | Yes | No | No |
| `JRT_MillisecondsToTicks()` | Yes | Yes | Yes | Yes | Yes |
| Task inspection and non-ISR synchronization, event, and memory-pool APIs | No | Yes | No | No | No |
| `JRT_TaskNotify()` and timer start/stop/restart/configuration | No | Yes | No | Yes | No |
| `JRT_KernelGetTickCount()`, `JRT_KernelTickReached()` | Yes | Yes | No | Yes | Yes |
| `JRT_SemaphoreGiveFromISR()` | No | No | No | No | Yes |
| `JRT_QueueSendFromISR()` | No | No | No | No | Yes |
| `JRT_TaskNotifyFromISR()` | No | No | No | No | Yes |
| `JRT_EventGroupSetBitsFromISR()` | No | No | No | No | Yes |

“Unprivileged task” in this table means a task configured with
`JRT_TASK_FLAG_UNPRIVILEGED` on an MPU-enabled target. Its supported kernel
entry points reside in unprivileged code or use the SVC gateways. Other public
kernel functions reside in privileged flash and are not callable directly by
such a task. Applications needing those services from unprivileged tasks must
use a privileged service task or add a separately reviewed SVC gateway; shared
data placement alone does not grant execution access to privileged functions.

Task-context synchronization calls detect ISR misuse and return failure or do
nothing as appropriate. Suspend and resume return
`JRT_STATUS_INVALID_CONTEXT`. The four `FromISR` functions reject Thread-mode
use, never block, and request PendSV when their operation can make a task
runnable. No other mutating kernel API is supported from an ISR, even if its
current implementation happens to use a critical section.

The Cortex-M port uses these exception priorities, where a lower numerical
value has higher urgency:

| Exception | Logical priority | Contract |
|---|---:|---|
| SVC | Reset priority 0 | Privileged gateway; must not be reprioritized by the application |
| Application maskable IRQ | 0 through 13 | May use only the four `FromISR` gateways and bounded read-only helpers |
| SysTick | 14 | Advances kernel time, processes expirations, invokes the optional tick hook, and pends PendSV |
| PendSV | 15 | Lowest priority; performs deferred context switching after all higher-priority handlers return |

Kernel critical sections save PRIMASK, disable every maskable interrupt, and
restore the previous PRIMASK value. They are nestable only through correctly
paired enter/exit calls and deliberately favor simple atomicity over selective
priority masking. NMI and fault handlers are not masked by PRIMASK and must not
call any kernel API. Application ISRs must be bounded: a long-running handler
delays SysTick when it has priority 0 through 13 and always delays PendSV, so a
woken higher-priority task cannot run until the handler chain completes.

The optional kernel tick hook executes inside SysTick at priority 14. It is a
diagnostic/test hook, must be bounded and non-blocking, and may use only the
same APIs permitted to a maskable ISR. It must not call task APIs or perform
application work that belongs in a task.

Timer callbacks are different from interrupt callbacks: they execute serially
in privileged Thread mode in the priority-1 timer-service task, outside the
kernel critical section. They must still be bounded and must not block, delay,
wait for synchronization, suspend, allocate/free memory-pool blocks, or call
`JRT_TimerDispatch()`. They may start, stop, restart, or reconfigure timers and
notify an application task. A callback that does not return prevents later
timer callbacks from running, while higher-priority application tasks may
still preempt it normally.

## Fatal-Error Policy

Processor faults, kernel-invariant failures, and software-detected task-stack
overflows retain their source-specific debugger records and then enter the
shared fatal path. The path masks all maskable interrupts, records
`g_fatal_active` and `g_fatal_reason`, and calls the weak
`JRT_FatalErrorHook()`. The default hook returns immediately to the kernel's
permanent halt loop.

An application may provide one strong definition of `JRT_FatalErrorHook()` to
record persistent diagnostics, signal an external watchdog, or request a
platform reset. The hook executes in privileged context with maskable
interrupts disabled. It must use only bounded, polling operations that are
safe in fault context; it must not call the scheduler, kernel APIs, blocking
drivers, or code that depends on interrupts. If the hook returns, the kernel
sets `g_fatal_hook_returned` and enters the same halt loop, so a faulty hook
cannot resume the scheduler or exception return path.

The hook receives a `JRT_FatalReason_t` value identifying processor fault,
kernel invariant, or stack overflow. Detailed data remains in
`g_fault_record`, the `g_kernel_invariant_*` fields, or the `g_stack_fault_*`
fields respectively. Reset and watchdog behavior is deliberately supplied by
the application because the correct mechanism and diagnostic-retention policy
are platform-specific.

## Port Boundary

Portable kernel code calls the contract in
`arch/cortex_m/port_contract.h` for critical sections, ISR detection, tick
startup, yielding, MPU setup, first-task startup, context switching, and idle
wait. Board-specific access stays below each `platform/<target>/board/`
directory and is reached from unprivileged tasks through the LED SVC gateway.

## Build and Validation

See [the testing guide](../tests/README.md) for build profiles and regression commands.

## Current Limitations


- Static task configuration only; no task creation or deletion. Suspension is
  limited to running or ready application tasks and does not cancel waits.
- Unprivileged writable data is either explicitly shared or a single
  power-of-two private region owned by one task; a task cannot declare several
  disjoint private regions.
- MPU isolation is enforced only on MPU-enabled targets. Privileged code and
  non-CPU bus masters such as DMA are outside the task-private access policy.
- MPU ranges use power-of-two sizes and matching base alignment.
- Fault handling records state and stops; it does not recover or reset.
- Timer callbacks run serially in the priority-1 kernel timer-service task and
  must not block, delay, or wait for synchronization.
- `JRT_TimerTakeExpirations()` remains available for callback-less polling
  timers. `JRT_TimerDispatch()` remains as a compatibility API; calling it
  explicitly executes claimed callbacks synchronously in the calling context.
- QEMU cannot exercise MPU isolation or floating-point context switching.

## Configuration Envelope

Application settings are selected in the application-owned `JRTConfig.h`.
The checked constraints
are summarized below; the default values are the reference values.

| Setting | Default | Supported constraint |
| --- | ---: | --- |
| `JRT_CORE_CLOCK_HZ` | 120000000 | Nonzero, fits `uint32_t` |
| `JRT_TICK_RATE_HZ` | 7500 | Nonzero, no greater than the core clock, produces a 24-bit SysTick reload |
| `JRT_MAX_APPLICATION_TASKS` | 7 | Nonzero and representable with three reserved scheduler slots |
| `JRT_DEFAULT_TASK_STACK_WORDS` | 128 | Even, no smaller than the architecture context, byte size fits `uint32_t` |
| `JRT_IDLE_STACK_WORDS` | 128 | Same stack constraints as application stacks |
| `JRT_TIMER_SERVICE_STACK_WORDS` | 128 | Same stack constraints as application stacks |
| `JRT_MAX_TASK_PRIORITY` | 31 | Nonzero and fits `uint32_t` |
| `JRT_TIMER_SERVICE_PRIORITY` | 1 | Above idle priority and no greater than the configured maximum |
| `JRT_ENABLE_TEST_HOOKS` | 0 | Either 0 or 1; production release uses 0 |
| `JRT_ENABLE_TASK_BENCHMARK` | 0 | Either 0 or 1; production release uses 0 |

Passing compile-time checks proves that a value is representable, not that it
is suitable for a particular application. The application must size every
stack from measured high-water use plus interrupt and call-depth margin,
account for the scheduler's linear scan as task capacity grows, and select a
tick rate consistent with its latency and CPU-load budget.

Release firmware uses `BUILD=release`, which selects `-O2 -g3 -DNDEBUG` and
keeps debugger information without disabling kernel invariant, stack, fault,
or fatal-path diagnostics.

## Application Integration

When integrating justRT into an application:

1. Keep task definitions, stacks, queues, synchronization objects, timers,
   and memory pools statically allocated for the lifetime expected by their
   APIs.
2. Give every unprivileged task only its required private aggregate. Place
   deliberately shared objects in `JRT_TASK_UNPRIVILEGED_DATA`; never store a
   secret in shared unprivileged memory.
3. Confirm task priorities, timer-service priority, blocking relationships,
   and worst-case callback work. Timer callbacks must remain bounded and must
   not block.
4. Measure stack high-water values under application worst-case load and add
   explicit safety margin. Do not use the architectural minimum as an
   application sizing recommendation.
5. Implement `JRT_FatalErrorHook()` only if the product needs persistent
   logging, watchdog handoff, or reset. The hook must be bounded and safe with
   maskable interrupts disabled.
6. Verify the target clock before starting the kernel and keep interrupt
   priorities consistent with the execution and ISR contract in `RTOS.md`.
7. Review all DMA and peripheral bus-master access separately; the CPU MPU
   does not protect private task data from those agents.
8. Build with `BUILD=release`, review the ELF size and map file, and retain the
   matching ELF for fault diagnosis.

## Residual Risks and Unsupported Features

- There is no dynamic task creation or deletion. Suspension supports only
  running and ready application tasks and does not cancel active waits.
- Faults are fail-stop. Recovery, reset, persistent logging, and watchdog
  policy belong to the application and platform.
- MPU enforcement is available only on MPU-enabled targets. Privileged code,
  DMA, and other bus masters can bypass task-private CPU access controls.
- Each task has at most one private power-of-two region with matching base
  alignment. General process-style address spaces are not provided.
- Software-timer callbacks execute serially in one privileged service task;
  a callback that fails to return prevents later callbacks from running.
- Task benchmarking is currently supported only on S32K312 hardware with a
  validated DWT cycle counter. QEMU rejects the real-time benchmark profile.
- Scheduler selection and several object wait-list operations are linear in
  configured task count. Larger task limits need application-specific timing
  measurement.
- QEMU does not validate S32K312 MPU behavior, floating-point context,
  startup timing, interrupt integration, peripherals, flash programming, or
  electrical behavior.
- The kernel has extensive deterministic regression coverage but is not a
  certified safety kernel and has no claimed compliance with a functional
  safety standard.
