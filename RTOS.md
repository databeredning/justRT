# JustBoot RTOS Notes

## 1. Purpose and Scope

This document describes the current RTOS foundation implemented in the JustBoot project for the S32K312 Cortex-M7 target.

The current code is a small, statically configured, preemptive kernel experiment. It is intentionally low level and currently focuses on:

- Cortex-M7 reset and boot integration.
- Process Stack Pointer (PSP) use for tasks.
- Main Stack Pointer (MSP) use for kernel and exception handling.
- Periodic SysTick interrupts.
- PendSV-based context switching.
- SVC-based task services.
- Static task stacks.
- Task states for ready, running, and sleeping tasks.
- A fallback idle task using `WFI`.
- Fault capture for HardFault, MemManage, BusFault, and UsageFault.
## 4. Core Execution Architecture

The kernel uses three Cortex-M7 exception paths around a static task model:

```text
Thread mode on PSP
    |  SVC: voluntary service request
    v
SVC handler on MSP
    |  service dispatch, then PendSV request
    v
PendSV handler on MSP
    |  save/restore r4-r11 and select the next task
    v
Thread mode on the selected task's PSP
```

### Vector table

`Vector_Table.s` places the immutable vector table in `.intc_vector`. The
linker aligns it at `__ROM_INTERRUPT_START`, and startup writes that address
to VTOR. The relevant entries are:

| Exception | Handler | Role |
|---|---|---|
| HardFault | `HardFault_Handler` | Fault capture fallback |
| MemManage | `MemManage_Handler` | MPU violation capture |
| BusFault | `BusFault_Handler` | Bus error capture |
| UsageFault | `UsageFault_Handler` | Invalid instruction/state capture |
| SVCall | `SVC_Handler` | Thread-mode kernel service entry |
| PendSV | `PendSV_Handler` | Deferred context switch |
| SysTick | `SysTick_Handler` | Tick accounting and reschedule request |

### `PendSV_Handler`

The naked handler runs with MSP and preserves the exception return token in
`lr` while the C scheduler selects the next task:

```asm
mrs     r0, psp
push    {r3, lr}
stmdb   r0!, {r4-r11}
bl      pendsv_switch
ldmia   r0!, {r4-r11}
msr     psp, r0
pop     {r3, lr}
bx      lr
```

`pendsv_switch()` saves the outgoing task PSP, normalizes its state, selects
the highest-priority ready task, marks it running, and returns its saved PSP.
PendSV is configured at the lowest exception priority so a SysTick or SVC
request completes before the switch occurs.

### `SVC_Handler`

The handler is implemented in `kernel/svc_cm7.s`. It uses bit 2 of
`EXC_RETURN` to select the hardware-stacked frame from MSP or PSP, calls
`svc_dispatch()`, restores the original exception return value, and exits with
`bx lr`. Kernel service implementations run privileged on MSP.

### `SysTick_Handler`

SysTick calls `tick_tasks()`, which advances the monotonic tick and updates
sleep, timeout, and timer state. It then requests PendSV. SysTick does not
perform a context switch directly.

### Initial task launch

`launch_first_task()` restores the first task's synthetic frame, writes its PSP,
clears PRIMASK while still privileged, and then applies the task's privilege
state through CONTROL. This ordering is required because unprivileged Thread
mode cannot clear PRIMASK.

## 5. Kernel API Reference

The following APIs are the public kernel contract. Functions marked as
ISR-safe never block; task-only functions must not be called from an
exception handler.

### Kernel and task control

```c
kernel_status_t kernel_init(const kernel_config_t *config);
void kernel_start(void);
kernel_status_t task_get_state(uint32_t task_id, task_state_t *state);
kernel_status_t task_get_stack_info(uint32_t task_id, task_stack_info_t *info);
kernel_status_t task_get_name(uint32_t task_id, const char **name);
kernel_status_t task_get_priority(uint32_t task_id, uint32_t *priority);
```

`kernel_init()` validates static task definitions and prepares their stacks.
`kernel_start()` enables SysTick and launches the first task. The inspection
functions return state, stack usage, name, or effective priority for a valid
task ID.

### Scheduling and time

```c
void yield(void);
void sleep_ticks(uint32_t ticks);
uint32_t ms_to_ticks(uint32_t milliseconds);
uint32_t kernel_ticks_now(void);
int kernel_tick_reached(uint32_t deadline);
void task_delay_until(uint32_t *previous_wake, uint32_t period_ticks);
```

`yield()` requests a reschedule through SVC. `sleep_ticks()` makes the current
task unavailable for the requested number of ticks. `kernel_ticks_now()` reads
the monotonic tick count. `kernel_tick_reached()` uses signed subtraction and
is safe across 32-bit tick wraparound. `task_delay_until()` advances a fixed
deadline, avoiding drift in periodic tasks.

### Port and kernel-internal entry points

```c
void tick_init(void);
void request_switch(void);
int kernel_in_isr(void);
uint32_t critical_enter(void);
void critical_exit(uint32_t saved_primask);
void tick_tasks(void);
void sleep_current(uint32_t ticks);
int task_block(void *object, task_wait_kind_t wait_kind, uint32_t timeout_ticks);
void task_wake(void *object, task_wait_kind_t wait_kind);
uint32_t task_current_index(void);
uint32_t task_current_priority(void);
void task_inherit_priority(uint32_t task_id, uint32_t priority);
void task_restore_priority(uint32_t task_id);
uint32_t *pendsv_switch(uint32_t *current_sp);
```

These functions support the port and synchronization implementations.
`critical_enter()` and `critical_exit()` save and restore PRIMASK. Blocking
functions change task state and rely on PendSV to select another task.

### Semaphores, mutexes, and queues

```c
void semaphore_init(semaphore_t *semaphore, uint32_t initially_available);
int semaphore_take(semaphore_t *semaphore, uint32_t timeout_ticks);
void semaphore_give(semaphore_t *semaphore);
void semaphore_give_from_isr(semaphore_t *semaphore);

void mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex, uint32_t timeout_ticks);
int mutex_unlock(mutex_t *mutex);

void queue_init(queue_t *queue, void *storage,
                uint32_t capacity, uint32_t item_size);
int queue_send(queue_t *queue, const void *item, uint32_t timeout_ticks);
int queue_receive(queue_t *queue, void *item, uint32_t timeout_ticks);
int queue_send_from_isr(queue_t *queue, const void *item);
```

Semaphore, mutex, and queue operations may block only in task context.
`semaphore_give_from_isr()` and `queue_send_from_isr()` are non-blocking.
The ISR queue send uses the documented drop-on-full policy. Mutexes implement
priority inheritance, including chained ownership restoration.

### Task notifications and event groups

```c
int task_notify(uint32_t task_id, uint32_t value);
int task_notify_from_isr(uint32_t task_id, uint32_t value);
int task_notify_take(uint32_t *value, uint32_t timeout_ticks);

void event_group_init(event_group_t *group);
uint32_t event_group_set_bits(event_group_t *group, uint32_t bits);
uint32_t event_group_set_bits_from_isr(event_group_t *group, uint32_t bits);
uint32_t event_group_wait_bits(event_group_t *group, uint32_t bits,
                               int wait_all, int clear_on_exit,
                               uint32_t timeout_ticks);
```

Notifications provide one accumulated value per task. Event groups provide
bit-based wait-any or wait-all synchronization and optional clear-on-exit.
Their ISR set functions do not block.

### Software timers

```c
void kernel_timer_init(kernel_timer_t *timer);
void kernel_timer_start(kernel_timer_t *timer, uint32_t delay_ticks);
void kernel_timer_start_periodic(kernel_timer_t *timer, uint32_t period_ticks);
void kernel_timer_restart(kernel_timer_t *timer);
void kernel_timer_set_callback(kernel_timer_t *timer,
                               kernel_timer_callback_t callback,
                               void *argument);
void kernel_timer_stop(kernel_timer_t *timer);
uint32_t kernel_timer_take_expirations(kernel_timer_t *timer);
void kernel_timer_dispatch(kernel_timer_t *timer);
```

Timers use monotonic deadlines and support one-shot and periodic operation.
SysTick records expirations; callbacks are dispatched by task code rather than
from interrupt context.

### Memory pools

```c
void memory_pool_init(memory_pool_t *pool, void *storage,
                      uint32_t block_size, uint32_t block_count,
                      uint32_t *used_bitmap);
void *memory_pool_alloc(memory_pool_t *pool);
int memory_pool_free(memory_pool_t *pool, void *block);
```

Memory pools allocate fixed-size blocks from caller-provided storage. They do
not use a heap, have bounded allocation behavior, reject invalid or
misaligned frees, and reject double frees.

## 6. Task Model

The task model is currently private to `kernel/task.c`.

### `task_entry_t`

```c
typedef void (*task_entry_t)(void *argument);
```

A task entry function receives its configured argument and does not return.
The current task bodies are infinite loops.

### `task_definition_t`

```c
typedef struct
{
    task_entry_t entry;
    void *argument;
    uint32_t stack_words;
    uint32_t priority;
    const char *name;
    uint32_t flags;
} task_definition_t;
```

Definitions are static application data. The kernel validates the entry,
stack size, and task count, while retaining ownership of stack storage and
scheduler state. Priorities are used by the current ready-task selection;
names and flags are reserved for diagnostics and future policy.

### Task states

```c
enum
{
    TASK_READY = 0U,
    TASK_RUNNING = 1U,
    TASK_SLEEPING = 2U,
    TASK_BLOCKED = 3U
};
```

#### `TASK_READY`

The task can be selected by the scheduler.

#### `TASK_RUNNING`

The task is the current task. There is only one current task on the active core.

#### `TASK_SLEEPING`

The task is delayed until its `sleep_ticks` counter reaches zero.

#### `TASK_BLOCKED`

The task is waiting on a synchronization object such as a semaphore, queue,
notification, mutex, or event group. It is removed from scheduler selection
until the object wakes it or its timeout expires.

### `task_t`

```c
typedef struct
{
    uint32_t *stack_bottom;
    uint32_t *stack_top;
    uint32_t *sp;
    uint32_t state;
    uint32_t sleep_ticks;
    uint32_t run_count;
    uint32_t *minimum_sp;
    uint32_t high_water_words;
    task_entry_t entry;
} task_t;
```

#### `stack_bottom`

Lowest address of the task's statically allocated stack.

#### `stack_top`

One-past-the-end address used when constructing the initial stack frame. Cortex-M stacks grow toward lower addresses.

#### `sp`

Saved Process Stack Pointer for the task. The PendSV handler stores the current task's saved PSP here and restores the next task's PSP from this field.

#### `state`

Current scheduler state: ready, running, or sleeping.

#### `sleep_ticks`

Remaining number of SysTick intervals before a sleeping task becomes ready.

#### `run_count`

Number of scheduler selections for the task. It is incremented when the task is switched out by `pendsv_switch()`.

#### `minimum_sp`

Lowest observed saved PSP for the task. It is initialized after the synthetic first-task frame is built and updated at every context switch.

#### `high_water_words`

Number of stack words that have been used according to the fill-pattern scan. The current stack fill pattern is `0xA5A5A5A5`.

#### `entry`

Task entry function associated with the task.

## 7. Static Tasks and Stacks

The kernel has eight statically allocated task slots: up to seven application
tasks plus one kernel-owned idle task. An application provides between one and
seven task definitions to `kernel_init()`; the kernel reserves the final slot
for idle.

```c
enum { APPLICATION_TASKS = 2U };
static const task_definition_t definitions[APPLICATION_TASKS] = { ... };
kernel_config_t config = { definitions, APPLICATION_TASKS };
```

Each stack contains `KERNEL_TASK_STACK_WORDS` 32-bit words, or 512 bytes by
default. The alignment attribute provides the 8-byte alignment required by the
Cortex-M exception and ABI conventions.

There is no heap allocation. Task storage and stacks are known at link time.

### Heartbeat example task

The heartbeat example toggles the run LED and sleeps for 750 ticks.

### Activity example task

The activity example periodically sleeps for seven ticks, exercising SVC
number one and the sleeping state.

### Idle task

The kernel idle task executes:

```c
__asm volatile ("wfi" : : : "memory");
```

The idle task is selected when all application tasks are sleeping or blocked.
`WFI` allows the core to wait for the next interrupt.

### `prepare_task()`

Initializes a task record from an application-provided entry:

1. Assigns each stack's bottom and top.
2. Builds an initial stack frame.
3. Sets the task state to ready.
4. Clears or initializes task metadata.
5. Assigns the entry function.

The kernel owns storage and scheduling; applications own task entry functions
and their selection. Future examples can provide a different entry table
without modifying kernel sources.

## 7.1 Native Board LED

Board-specific hardware is kept outside the kernel in `board/`:

```c
void board_init(void);
void board_led_toggle(void);
```

The current native implementation configures PTB18 as a SIUL2 GPIO output and toggles its GPDO value. It uses:

```text
SIUL2 base:       0x40290000
PTB18 SIUL2 pin:  50 (port B offset 32 + pin 18)
MSCR OBE:         bit 21
```

`heartbeat_example_start()` calls `board_init()` before `kernel_start()`.
The heartbeat task calls `board_led_toggle()` and then sleeps for 750 RTOS
ticks, approximately 100 ms at the current clock. This keeps board setup and
application behavior outside the kernel.

The implementation assumes the board LED is connected directly to PTB18, GPIO is the default SIUL2 signal, and the LED is active-high. If the LED is active-low, invert `led_state` before writing GPDO. If the board uses a different SIUL2 register map or pin mux configuration, only `board/board.c` should change.

When NXP RTD is introduced, keep this interface and replace the register operations with the generated Port/Dio calls. The kernel should not include RTD headers.

## 8. Stack Safety Instrumentation

Each task stack is initialized with `TASK_STACK_FILL` before its synthetic startup frame is built:

```c
#define TASK_STACK_WORDS 128U
#define TASK_STACK_FILL 0xA5A5A5A5U
```

`update_stack_usage()` runs when PendSV saves a task. It performs three checks:

1. The saved PSP must be at or above `stack_bottom`.
2. The saved PSP must be at or below `stack_top`.
3. The saved PSP must be 8-byte aligned.

It then updates `minimum_sp` and scans upward from `stack_bottom` until it finds the first untouched fill-pattern word. The distance from that word to `stack_top` is stored in `high_water_words`.

On a violation, the kernel records the task and stack address, then stops:

```c
g_stack_fault      = 1U;
g_stack_fault_task = current task index;
g_stack_fault_sp   = offending PSP;
```

The task storage also reserves an aligned 32-byte no-access MPU guard below
each stack. `configure_stack_guards()` enables three MPU regions before the
first task launches. A downward stack overflow therefore raises a MemManage
fault before it reaches another task's storage. The software bounds check is
still retained for saved-PSP validation.

## 9. Initial Task Stack Frame

### `build_initial_stack()`

```c
static uint32_t *build_initial_stack(uint32_t *stack_top, task_entry_t entry);
```

Constructs the synthetic stack expected by the context-switch path.

The frame contains two parts:

```text
low address
+-----------------------------+
| saved r4                    |
| saved r5                    |
| saved r6                    |
| saved r7                    |
| saved r8                    |
| saved r9                    |
| saved r10                   |
| saved r11                   |
+-----------------------------+
| r0                          |
| r1                          |
| r2                          |
| r3                          |
| r12                         |
| LR = task_exit_trap         |
| PC = task entry             |
| xPSR = 0x01000000           |
+-----------------------------+
high address
```

The software frame is restored by PendSV using `ldmia {r4-r11}`. The hardware-shaped frame is used by the first-task launcher to obtain the entry and return addresses.

The stored entry PC is made even because the first-task launcher explicitly sets the Thumb bit before using `BX`. The stored `xPSR` has the Thumb bit set.

### `task_exit_trap()`

A task is not allowed to return. If a task entry ever returns, its synthetic LR points to `task_exit_trap()`, which currently enters a permanent loop. A future version should convert this into a task termination service.

### `launch_first_task()`

A naked Cortex-M7 assembly function used only for the initial task launch. It:

1. Receives the first task's saved stack pointer in `r0`.
2. Restores `r4-r11`.
3. Loads the synthetic LR and task PC.
4. Sets the Thumb bit on the direct branch target.
5. Advances past the hardware-shaped frame.
6. Writes the resulting task stack position to PSP.
7. Sets `CONTROL.SPSEL` so Thread mode uses PSP.
8. Executes `ISB` after changing CONTROL.
9. Enables interrupts.
10. Clears selected argument registers.
11. Branches to the task entry.

This path is separate from exception return because the first task is launched directly from kernel startup rather than resumed from an exception.

## 10. Scheduler and Context Switching

### Scheduler selection

`pendsv_switch()` selects the highest-priority ready task. Starting after the
current task, it selects the first task with that priority, preserving
round-robin behavior among equal-priority tasks. Sleeping and blocked tasks
are excluded.

The scheduler currently treats every non-sleeping task as selectable. There is no separate check for `TASK_READY` versus `TASK_RUNNING`, because the current state set is small and the current task is normalized before selection.

### PSP and MSP roles

The kernel uses the Cortex-M stack split as follows:

- MSP: reset handler, kernel startup, interrupt handlers, and exception entry.
- PSP: task execution and task-saved context.

This separation is a prerequisite for later unprivileged task execution and MPU enforcement.

### `PendSV_Handler`

The handler is naked assembly because it must control the exact register and stack sequence:

```text
mrs     r0, psp
push    {r3, lr}          preserve EXC_RETURN and maintain alignment
stmdb   r0!, {r4-r11}     save software task context
bl      pendsv_switch     choose next task
ldmia   r0!, {r4-r11}     restore next task context
msr     psp, r0
pop     {r3, lr}
bx      lr                exception return
```

The `push {r3, lr}` pair is important. Saving only `lr` would misalign MSP during the C call. The saved `lr` value is the Cortex-M `EXC_RETURN` token and must survive the call to `pendsv_switch()`.

### `g_current_task_index`

Current task slot index. Application slots come first and the final configured
slot is idle:

- `0` through `task_count - 2`: application tasks.
- `task_count - 1`: idle task.

## 11. SysTick and Timekeeping

### `tick_init()`

Programs the system timer:

```text
SysTick reload = 15999
clock source  = processor clock
tick interrupt enabled
SysTick enabled
```

It also sets exception priorities through `SCB_SHPR3`:

- PendSV logical priority: `0x0F`, encoded as `0xF0`, lowest.
- SysTick logical priority: `0x0E`, encoded as `0xE0`, slightly higher.

S32K312 implements four priority bits, stored in the upper nibble of each
priority byte. The lower nibble is not implemented, so using `0xFF` and
`0xFE` would produce the same effective priority. The explicit encoding above
preserves the intended SysTick-before-PendSV ordering.

This ordering allows SysTick to request a context switch while PendSV performs the switch later at the lowest priority.

### `SysTick_Handler()`

On every timer tick it:

1. Calls `tick_tasks()` to decrement sleep counters.
2. Pends PendSV through `request_switch()`.

## 12. SVC Services

### SVC instruction numbers

The current ABI is:

| SVC number | Service | Meaning |
|---:|---|---|
| `0` | yield | Voluntary reschedule request |
| `1` | sleep | Block current task for `r0` ticks |
| `2` | led_toggle | Toggle PTB18 through privileged board code |

### `yield()`

Implemented in `kernel/port_cm7.c` as:

```c
__asm volatile ("svc 0" : : : "memory");
```

It does not directly switch context. It enters SVC and pends PendSV.

### `sleep_ticks()`

Loads the requested tick count into `r0` and executes `svc 1`:

```c
register uint32_t argument asm("r0") = ticks;
__asm volatile ("svc 1" : "+r" (argument) : : "memory");
```

The SVC handler later reads the stacked `r0` value.

### `SVC_Handler`

A naked handler selects the correct exception stack based on `EXC_RETURN` bit 2:

```text
if LR bit 2 is clear: use MSP
if LR bit 2 is set:   use PSP
```

It branches to `svc_dispatch()` with the selected stacked frame.

### `svc_dispatch()`

Reads the SVC instruction number from the instruction immediately before the stacked PC:

```c
uint8_t svc_number = ((const uint8_t *)stacked_frame[6])[-2];
```

For SVC number one it calls `sleep_current(stacked_frame[0])`. SVC number two
calls the privileged board LED routine. SVC number zero requests a reschedule;
unknown numbers are rejected.

The dispatcher also rejects SVC requests originating from handler mode. Such
requests increment `g_svc_invalid_context` and do not request a context
switch.

After dispatch it always requests PendSV.

### Important SVC limitations

The current dispatcher is intentionally minimal:

- SVC calls do not yet validate the caller or all arguments.
- There is no pointer validation because no pointer-bearing service exists yet.
- There is no return-value convention.
- The handler assumes a standard eight-word exception frame and does not yet handle the optional floating-point extended frame.

## 13. Fault Handling

Fault handling is implemented in `kernel/fault.c`.

### `fault_record_t`

```c
typedef struct
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
    uint32_t exc_return;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t dfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t afsr;
    uint32_t fault_type;
} fault_record_t;
```

### `g_fault_record`

A volatile global record intended for post-fault inspection.

The stacked registers identify the interrupted instruction and execution context. The SCB registers identify the processor fault cause.

### `g_fault_active`

Set to `1` after a fault has been captured. The capture function then enters a permanent loop so the record remains stable for recovery handling.

### `fault_capture()`

Copies the exception frame and fault status registers into `g_fault_record`.

Arguments:

```c
void fault_capture(uint32_t *stacked_frame,
                   uint32_t exc_return,
                   uint32_t fault_type);
```

### Strong fault handlers

The following handlers are implemented as naked wrappers that select MSP or PSP and branch to `fault_capture()`:

- `HardFault_Handler`: fault type `1`.
- `MemManage_Handler`: fault type `2`.
- `BusFault_Handler`: fault type `3`.
- `UsageFault_Handler`: fault type `4`.

### Default handlers

`system.c` still provides `undefined_handler()` and weak aliases for handlers that have not yet received specialized implementations, including NMI and DebugMonitor.

## 14. Runtime Initialization

### `init_data_bss()`

Defined in `system.c`. It consumes linker-generated initialization tables:

- `__INIT_TABLE`: flash-to-RAM copy ranges.
- `__ZERO_TABLE`: RAM ranges to clear.

It copies initialized data and zeros BSS-like regions before normal C code relies on them.

### `SystemInit()`

Currently empty. Board clock and peripheral setup remains outside the kernel;
the kernel performs its MPU setup during task initialization.

## 15. Linker and Memory Considerations

The linker script currently defines separate regions for:

- Program flash.
- ITCM.
- DTCM.
- General SRAM.
- Stack SRAM.
- Non-cacheable SRAM.
- Shareable SRAM.
- Interrupt vector RAM.

The startup assembly initializes ECC and TCM regions before entering C code.

The linker uses explicit ownership sections for task storage and kernel data.
The current layout defines symbols for:

- Kernel stack.
- Per-task stack regions.
- Guard gaps and no-access regions.
- Privileged kernel data.
- Unprivileged task data.

## 16. Current Guarantees

The current design provides these useful guarantees:

- Task stacks are statically allocated.
- Task context is switched through PSP.
- Kernel and exception entry use MSP.
- PendSV is lower priority than SysTick.
- Task 1 cannot continue running while its state is sleeping.
- An idle task is available as a scheduler fallback.
- Fault handlers capture the stacked frame and SCB fault status before stopping.
- The build is freestanding and does not depend on a C runtime or standard library.

## 17. Current Limitations

The following limitations are known and intentional at this stage:

1. The scheduler has a fixed maximum of `KERNEL_MAX_TASKS` slots.
The current default is `8` total slots: up to seven worker tasks plus idle.
2. Task stacks are bounded by `KERNEL_TASK_STACK_WORDS` words.
3. There is no public dynamic task creation or task deletion API.
4. Timer callbacks run through explicit dispatch task code; no general timer service task exists.
5. SVC pointer-bearing services and return-value conventions are not defined.
6. The memory pool has no ownership or allocation-statistics API.
7. Fault handlers do not yet capture the floating-point extended frame.
8. Watchdog servicing and recovery are not integrated.
9. Cache maintenance and memory attributes are not part of the kernel API.

## 18. Recommended Development Order

The next low-level milestones should be implemented in this order:

### 18.1 Stack validation

Stack watermarking, scheduler-time bounds checks, and MPU no-access guard
regions below each task stack are implemented. The next refinement is to
define a reset and recovery policy for captured MemManage records.

### 18.2 Critical-section primitives

Architecture-specific PRIMASK helpers are present and protect task state, sleep accounting, and scheduler selection. The next refinement is to define which APIs are legal from thread mode, SVC, SysTick, and PendSV context.

### 18.3 Board and RTD boundary

The native board layer is now present for the PTB18 heartbeat. RTD should be introduced when additional production peripheral services are needed, such as clock, pin, GPIO, watchdog, CAN, or ADC configuration. Replace board implementations behind the same interface rather than coupling RTD to kernel code.

### 18.4 Fault hardening

Add fault nesting detection, a reset policy, and persistent fault storage in a reserved RAM or data-flash region.

### 18.5 MPU setup

The MPU now configures explicit flash, SRAM, unprivileged-function,
unprivileged-read-only-data, and unprivileged-task-data regions during kernel
initialization. Guard regions use the higher MPU region numbers so their
no-access priority overrides the general SRAM and task-data regions.

`PRIVDEFENA` remains enabled and tasks still launch privileged. This stage
validates the memory layout and attributes without changing the execution
privilege level.

The linker now reserves named ranges for privileged functions, unprivileged
functions, unprivileged read-only data, privileged data, unprivileged task data, and system-call
trampolines. Source declarations use `KERNEL_PRIVILEGED`,
`TASK_UNPRIVILEGED`, `KERNEL_PRIVILEGED_DATA`, and
`TASK_UNPRIVILEGED_RODATA`, and `TASK_UNPRIVILEGED_DATA` attributes to express ownership; the linker only
collects those named sections. This preparation does not enable
unprivileged execution or change MPU permissions. Existing task stacks and
guard regions are the first storage assigned to the new task-data range.

### 18.6 Privilege transition

Tasks may launch privileged or unprivileged according to their task flags.
Future services must continue to cross the validated SVC boundary.

### 18.7 Time services

Add a monotonic tick type, timeout comparison helpers, and a defined tick-wrap policy.

### 18.8 Binary semaphore

The kernel now provides a static binary semaphore:

```c
semaphore_t semaphore;
semaphore_init(&semaphore, 0U);
```

`semaphore_take()` returns immediately for a zero timeout, retries once per
tick for a finite timeout, and waits indefinitely with
`SEMAPHORE_WAIT_FOREVER`. A task that cannot take the semaphore enters the
kernel `TASK_BLOCKED` state rather than polling. `semaphore_give()` publishes
the token and wakes the matching blocked task under a PRIMASK critical
section. These APIs are currently intended for task context; ISR-specific
give and take services are not yet defined.

### 18.9 Mutex

The kernel provides a static, task-owned mutex:

```c
mutex_t mutex;
mutex_init(&mutex);
mutex_lock(&mutex, SEMAPHORE_WAIT_FOREVER);
/* protected resource */
mutex_unlock(&mutex);
```

Only the owning task may unlock the mutex. Recursive locking by the owner is
allowed and increments a recursion count; matching unlocks are required before
release. Unlocking by another task returns failure. A contending task enters `TASK_BLOCKED` until the
owner releases the mutex or the timeout expires. When a higher-priority task
blocks on the mutex, the owner temporarily inherits that priority and returns
to its base priority on unlock. Nested mutex priority chains are not yet
implemented.

### 18.10 IPC

The kernel now provides a bounded static byte queue. The caller owns the
storage and initializes it with a capacity and fixed item size:

```c
uint8_t storage[4 * sizeof(uint32_t)];
queue_t queue;
queue_init(&queue, storage, 4U, sizeof(uint32_t));
```

`queue_send()` and `queue_receive()` use the same zero, finite, and
`SEMAPHORE_WAIT_FOREVER` timeout meanings as the semaphore. Full senders and
empty receivers enter `TASK_BLOCKED`; a successful receive wakes a blocked
sender and a successful send wakes a blocked receiver. When multiple tasks
wait on the same object, the highest-priority waiter is selected; equal
priorities retain static slot order. Ring-buffer state and item copies are
protected by PRIMASK. These APIs are currently intended for task context;
ISR-specific operations are not yet defined.

For interrupt context, use `semaphore_give_from_isr()` and
`queue_send_from_isr()`. These APIs never block and explicitly pend PendSV via
`request_switch()` after waking waiters.

`queue_send_from_isr()` uses a fixed drop-on-full backpressure policy. When the
queue is full, the send returns failure and does not block. The kernel exposes
`g_isr_queue_send_attempted`, `g_isr_queue_send_accepted`,
`g_isr_queue_send_dropped`, and `g_isr_queue_count_high_water` for runtime
verification of ISR traffic and saturation behavior.

Context-guard misuse is tracked both as an aggregate
(`g_sync_context_misuse`) and as per-API counters:
`g_sync_misuse_semaphore_take`, `g_sync_misuse_semaphore_give`,
`g_sync_misuse_semaphore_give_from_isr`, `g_sync_misuse_mutex_lock`,
`g_sync_misuse_mutex_unlock`, `g_sync_misuse_queue_send`,
`g_sync_misuse_queue_receive`, and `g_sync_misuse_queue_send_from_isr`.

### 18.10 Synchronization example

`examples/sync_producer_consumer.c` provides a selectable producer/consumer
application. The producer sends incrementing values into a bounded queue and
gives a semaphore after each successful send. The consumer takes the
semaphore, receives from the queue, and records FIFO mismatches in
`g_sync_error`. `g_sync_producer_value` and `g_sync_consumer_value` expose
producer and consumer progress.

The default `main.c` continues to select the heartbeat example. To run this
example, select `sync_producer_consumer_start()` from `main()` instead.

### 18.11 Semaphore event example

`examples/semaphore_event.c` demonstrates semaphore-only event notification.
The event source gives a binary semaphore every 100 ms. The worker blocks on
`semaphore_take()` and increments its received counter when the event arrives.
The runtime counters are `g_semaphore_events_sent`,
`g_semaphore_events_received`, and `g_semaphore_event_error`.

The default `main.c` continues to select the queue example. To run this
example, select `semaphore_event_start()` from `main()` instead.

### 18.12 Mutex contention example

`examples/mutex_contention.c` demonstrates mutex ownership and contention.
The owner and contender update a shared counter only while holding the mutex.
The runtime counters are `g_mutex_owner_operations`,
`g_mutex_contender_operations`, `g_mutex_error`, `g_mutex_contender_state`,
`g_mutex_contender_state_after_unlock`, and `g_mutex_contender_stack_used`.
The two state values show the contender blocked while the mutex is held and
ready immediately after the owner wakes it. A nornero error indicates failed
ownership, timeout, or inspection behavior.

The default `main.c` continues to select the semaphore example. To run this
example, select `mutex_contention_start()` from `main()` instead.

### 18.13 Mutex priority-inheritance example

`examples/mutex_priority_inheritance.c` starts a low-priority mutex owner, a
medium-priority CPU task, and a high-priority waiter. When the waiter blocks,
the owner inherits the waiter's effective priority and runs ahead of the
medium task until it unlocks the mutex. Inspect
`g_inheritance_low_priority`, `g_inheritance_high_state`,
`g_inheritance_low_operations`, `g_inheritance_high_operations`,
`g_inheritance_medium_operations`, and `g_inheritance_error` as runtime state.

The default `main.c` continues to select the semaphore example. To run this
example, select `mutex_priority_inheritance_start()` from `main()` instead.

### 18.14 Task inspection

The kernel exposes read-only diagnostic queries for each static task ID:

```c
task_get_state(id, &state);
task_get_stack_info(id, &stack_info);
task_get_name(id, &name);
task_get_priority(id, &priority);
```

The results are protected by a short PRIMASK critical section and represent a
consistent snapshot at the time of the query. Invalid IDs or null output
pointers return `KERNEL_ERR_INVALID_TASK`. Task IDs are stable slot indexes;
the current configuration uses worker tasks first and the kernel idle task in
the final slot.

The kernel also exposes runtime state counters:
`g_context_switches`, `g_ready_scan_depth_max`,
`g_sched_pass1_iters_total`, `g_sched_pass2_iters_total`,
`g_sched_pass2_iters_max`,
`g_wait_timeout_semaphore`, `g_wait_timeout_queue_send`,
`g_wait_timeout_queue_receive`, `g_wait_timeout_mutex`, and
`g_sync_context_misuse`.

### 18.15 Watchdog integration

The kernel now increments `g_idle_kicks` on every idle-loop pass before `WFI`.
This provides a software heartbeat that confirms the
scheduler is still making progress when the system is otherwise idle.

### 18.16 Mutex edge-case example

`examples/mutex_edge_cases.c` verifies recursive lock/unlock behavior and
rejects unlock attempts by a non-owner. Inspect
`g_mutex_recursive_first_lock`, `g_mutex_recursive_second_lock`,
`g_mutex_recursive_first_unlock`, `g_mutex_recursive_second_unlock`,
`g_mutex_non_owner_unlock`, and `g_mutex_edge_error`. All lock/unlock results
should be `1` except `g_mutex_non_owner_unlock`, which should be `0`; the
error value should remain `0`.

### 18.17 Waiter priority-wake example

`examples/waiter_priority_wake.c` validates wake ordering when two tasks block
on the same semaphore. The first give must wake the higher-priority waiter and
the second give must wake the lower-priority waiter. Inspect
`g_waiter_wake_order[0]`, `g_waiter_wake_order[1]`, `g_waiter_wake_count`,
`g_waiter_wake_error`, and `g_waiter_wake_done`. The expected pass result is:
`g_waiter_wake_done == 1`, `g_waiter_wake_error == 0`,
`g_waiter_wake_order[0] == 0xA1`, and `g_waiter_wake_order[1] == 0xB2`.

### 18.18 Waiter timeout and wake-order example

`examples/waiter_timeout_wake.c` validates timeout interaction with wake
selection. A high-priority task blocks with a finite timeout and must time
out, then gives the semaphore once while two lower-priority waiters remain
blocked. The wake must select the highest-priority remaining waiter first.
Inspect `g_waiter_timeout_flag`, `g_waiter_timeout_wake_order[0]`,
`g_waiter_timeout_wake_count`, `g_waiter_timeout_error`, and
`g_waiter_timeout_done`. The expected pass result is:
`g_waiter_timeout_flag == 1`, `g_waiter_timeout_done == 1`,
`g_waiter_timeout_error == 0`, and
`g_waiter_timeout_wake_order[0] == 0xC3`.

### 18.19 Multi-mutex priority restore example

`examples/mutex_multi_restore.c` validates that priority inheritance restore
is recalculated across all currently owned mutexes. The owner task takes two
mutexes, a high-priority waiter blocks on one mutex, and a medium-priority
waiter blocks on the other. After the first unlock, the owner priority must
drop from high to medium; after the second unlock, it must drop to base.
Inspect `g_multi_restore_owner_priority_before_release`,
`g_multi_restore_owner_priority_after_first_release`,
`g_multi_restore_owner_priority_after_second_release`,
`g_multi_restore_high_waiter_acquired`, `g_multi_restore_mid_waiter_acquired`,
`g_multi_restore_error`, and `g_multi_restore_done`. The expected pass result
is `3`, `2`, and `1` for the three priority snapshots, both acquired flags set
to `1`, `g_multi_restore_error == 0`, and `g_multi_restore_done == 1`.

### 18.20 Chained mutex inheritance example

`examples/mutex_chain_inheritance.c` validates transitive inheritance through
a wait chain. A low-priority owner holds `mutex_1`, a medium-priority bridge
holds `mutex_2` and blocks on `mutex_1`, and a high-priority task blocks on
`mutex_2`. The owner must inherit the high priority through the bridge task.
Inspect `g_chain_owner_priority_after_chain`, `g_chain_bridge_blocked`,
`g_chain_high_blocked`, `g_chain_bridge_acquired_mutex_1`,
`g_chain_high_acquired_mutex_2`, `g_chain_error`, and `g_chain_done`.
Expected pass values are owner priority `3`, all block/acquire flags set to
`1`, `g_chain_error == 0`, and `g_chain_done == 1`.

### 18.21 Mutex timeout priority-restore example

`examples/mutex_timeout_restore.c` validates that a waiter timeout propagates
priority recalculation up the ownership chain. A low-priority owner holds
`mutex_1`; a medium-priority bridge holds `mutex_2` and blocks on `mutex_1`
with no timeout; a high-priority task blocks on `mutex_2` with a finite
timeout. While the full chain exists the owner must be boosted to high
priority; after the high-priority waiter times out, the owner must drop
back to medium (the bridge still blocks on `mutex_1`) — not all the way
to base. Inspect `g_timeout_restore_owner_priority_full_chain`,
`g_timeout_restore_owner_priority_after_timeout`,
`g_timeout_restore_bridge_still_blocked`, `g_timeout_restore_error`, and
`g_timeout_restore_done`. Expected pass values: `3`, `2`, bridge flag `1`,
error `0`, done `1`.

### 18.22 ISR synchronization API example

`examples/isr_sync_paths.c` validates `semaphore_give_from_isr()` and
`queue_send_from_isr()` using a real SysTick interrupt hook. The interrupt
periodically gives a semaphore and enqueues increasing values; a consumer task
blocks on these objects and verifies monotonic queue data. A monitor task sets
completion once enough interrupt events are consumed.

Inspect `g_isr_sync_irq_give_count`, `g_isr_sync_irq_queue_sent`,
`g_isr_sync_irq_queue_dropped`, `g_isr_sync_sem_taken`,
`g_isr_sync_queue_received`, `g_isr_sync_error`, and `g_isr_sync_done`.
Expected pass behavior: done becomes `1`, error stays `0`, drops stay `0`, and
sent counts match received counts.
