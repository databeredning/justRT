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
- Debugger-visible counters and boot markers.
- Fault capture for HardFault, MemManage, BusFault, and UsageFault.

This is not yet a production RTOS. It does not yet provide complete task
memory isolation, queues, semaphores, interrupt-safe APIs, watchdog
integration, or a general public task creation API.

## 2. Source Layout

The current source layout is:

```text
main.c                         Platform/application entry point
system.c                       Runtime data/BSS initialization and default handlers
startup_cm7.s                  Reset sequence, stack setup, ECC/TCM initialization
Vector_Table.s                 Cortex-M vector table
linker_flash_s32k312.ld       Flash/SRAM layout and linker symbols
kernel/kernel.h                Kernel-facing declarations
kernel/task.c                  Task model, stacks, scheduler, task bodies
kernel/port_cm7.c              SysTick, SVC, PendSV, and Cortex-M7 instructions
kernel/fault.c                 Fault frame and system register capture
board/board.h                  Board-facing LED interface
board/board.c                  Native SIUL2 PTB18 implementation
Makefile                      Cross-compilation and link rules
```

The `kernel` directory is the logical namespace for kernel code. Function names are intentionally short and generic within that directory, for example `start`, `yield`, `tick_init`, and `pendsv_switch`.

## 3. Boot and Startup Flow

The high-level execution flow is:

```text
Reset_Handler
    |
    +-- mask interrupts
    +-- enable required early clocks
    +-- point VTOR to the flash interrupt table
    +-- select the core stack
    +-- disable the startup watchdog on core 0
    +-- initialize SRAM ECC
    +-- initialize DTCM and ITCM
    +-- initialize .data and .bss through init_data_bss()
    +-- call SystemInit()
    +-- call main()
             |
             +-- start()
                      |
                      +-- prepare_tasks()
                      +-- tick_init()
                      +-- launch_first_task()
                               |
                               +-- task 0 starts on PSP
```

### 3.1 `main.c`

#### `main()`

The entry point calls `start()`. `start()` is expected never to return.


## 4. Public Kernel Declarations

The declarations shared by the kernel files are in `kernel/kernel.h`.

### `start()`

```c
void start(void);
```

Starts the kernel. It prepares the static task slots, selects task zero, enables the tick source, and launches the first task.

This function is called from `main()` after platform sanity checks.

### `yield()`

```c
void yield(void);
```

Requests a voluntary reschedule through SVC number zero and pends PendSV.

The task continues after the SVC instruction when it is eventually scheduled again.

### `sleep_ticks()`

```c
void sleep_ticks(uint32_t ticks);
```

Requests that the current task sleep for a number of SysTick intervals. It uses SVC number one and passes the tick count in `r0`.

A zero duration does not block the task.

### `tick_init()`

Configures and enables the Cortex-M SysTick peripheral.

The current reload value is `15999`, which is a hardware-clock-dependent interval. It should eventually be replaced by a platform clock-derived configuration.

### `request_switch()`

Sets the PendSV pending bit in SCB ICSR. PendSV performs the actual context switch at the lowest configured exception priority.

### `critical_enter()` and `critical_exit()`

These Cortex-M7 port primitives provide a save-and-restore interrupt mask boundary using PRIMASK:

```c
uint32_t saved_primask = critical_enter();
/* protected kernel state access */
critical_exit(saved_primask);
```

`critical_enter()` returns the previous PRIMASK value before executing `CPSID I`. `critical_exit()` restores that exact value rather than blindly enabling interrupts, preserving an already-disabled outer critical section.

These primitives are applied around task state changes, sleep accounting, and scheduler selection. Critical sections should remain short and must not be used around task bodies or blocking operations.

### `tick_tasks()`

Called by SysTick. It decrements the sleep counter for every sleeping task and changes tasks whose counter reaches zero back to `TASK_READY`.

### `sleep_current()`

```c
void sleep_current(uint32_t ticks);
```

Changes the current task to `TASK_SLEEPING` when `ticks` is nonzero. A zero value leaves the task ready.

This function is called from the SVC dispatcher rather than directly by task code.

### `pendsv_switch()`

```c
uint32_t *pendsv_switch(uint32_t *current_sp);
```

C-level scheduler helper called by the naked PendSV handler.

It:

1. Saves the current task's software stack pointer.
2. Increments its run count.
3. Changes a running task back to ready unless it has already been put to sleep.
4. Searches for the next non-sleeping task.
5. Updates `current_task` and `g_current_task_index`.
6. Marks the selected task as running.
7. Returns the selected task's saved stack pointer.

## 5. Task Model

The task model is currently private to `kernel/task.c`.

### `task_entry_t`

```c
typedef void (*task_entry_t)(void);
```

A task entry function takes no arguments and does not return. The current task bodies are infinite loops.

### Task states

```c
enum
{
    TASK_READY = 0U,
    TASK_RUNNING = 1U,
    TASK_SLEEPING = 2U
};
```

#### `TASK_READY`

The task can be selected by the scheduler.

#### `TASK_RUNNING`

The task is the current task. There is only one current task on the active core.

#### `TASK_SLEEPING`

The task is blocked until its `sleep_ticks` counter reaches zero.

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

## 6. Static Tasks and Stacks

The current kernel has three statically allocated task slots:

```c
static uint32_t task0_stack[128] __attribute__((aligned(8)));
static uint32_t task1_stack[128] __attribute__((aligned(8)));
static uint32_t idle_stack[128] __attribute__((aligned(8)));
```

Each stack contains 128 32-bit words, or 512 bytes. The alignment attribute provides the 8-byte alignment required by the Cortex-M exception and ABI conventions.

There is no heap allocation. Task storage and stacks are known at link time.

### Task 0

`task0_body()` toggles the run LED and sleeps for 100 ticks.

### Task 1

`task1_body()` periodically sleeps for seven ticks, exercising SVC number one
and the sleeping state.

### Idle task

`idle_body()` executes:

```c
__asm volatile ("wfi" : : : "memory");
```

The idle task is selected when the worker tasks are sleeping or otherwise unavailable. `WFI` allows the core to wait for the next interrupt.

### `prepare_tasks()`

Initializes all three task records explicitly:

1. Assigns each stack's bottom and top.
2. Builds an initial stack frame.
3. Sets the task state to ready.
4. Clears or initializes task metadata.
5. Assigns the entry function.

The current implementation intentionally avoids a generalized task creation API while the low-level scheduler is still being developed.

## 6.1 Native Board LED

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

`board_init()` is called by `start()` before task stacks are prepared. Task 0 calls `board_led_toggle()` and then sleeps for 750 RTOS ticks, approximately 100 ms at the current clock. This makes the LED heartbeat independent of compiler-dependent loop speed and avoids placing peripheral register knowledge in the scheduler.

The implementation assumes the board LED is connected directly to PTB18, GPIO is the default SIUL2 signal, and the LED is active-high. If the LED is active-low, invert `led_state` before writing GPDO. If the board uses a different SIUL2 register map or pin mux configuration, only `board/board.c` should change.

When NXP RTD is introduced, keep this interface and replace the register operations with the generated Port/Dio calls. The kernel should not include RTD headers.

## 7.1 Stack Safety Instrumentation

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

On a violation, the kernel sets these debugger-visible globals and stops:

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

## 7. Initial Task Stack Frame

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

## 8. Scheduler and Context Switching

### Scheduler selection

`pendsv_switch()` performs a simple round-robin search over three slots. Starting after the current task, it checks up to three positions and selects the first task whose state is not `TASK_SLEEPING`.

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

Debugger-visible index of the current task slot. The current slots are:

- `0`: worker task 0.
- `1`: worker task 1.
- `2`: idle task.

## 9. SysTick and Timekeeping

### `tick_init()`

Programs the system timer:

```text
SysTick reload = 15999
clock source  = processor clock
tick interrupt enabled
SysTick enabled
```

It also sets exception priorities through `SCB_SHPR3`:

- PendSV priority: `0xFF`, lowest.
- SysTick priority: `0xFE`, slightly higher.

This ordering allows SysTick to request a context switch while PendSV performs the switch later at the lowest priority.

### `SysTick_Handler()`

On every timer tick it:

1. Calls `tick_tasks()` to decrement sleep counters.
2. Pends PendSV through `request_switch()`.

## 10. SVC Services

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

After dispatch it always requests PendSV.

### Important SVC limitations

The current dispatcher is intentionally minimal:

- SVC calls do not yet validate the caller or all arguments.
- There is no pointer validation because no pointer-bearing service exists yet.
- There is no return-value convention.
- The handler assumes a standard eight-word exception frame and does not yet handle the optional floating-point extended frame.

## 11. Fault Diagnostics

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

A volatile global record intended for debugger inspection after a fault.

The stacked registers identify the interrupted instruction and execution context. The SCB registers identify the processor fault cause.

### `g_fault_active`

Set to `1` after a fault has been captured. The capture function then enters a permanent loop so the debugger can inspect the record without the system continuing and overwriting it.

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

## 12. Runtime Initialization

### `init_data_bss()`

Defined in `system.c`. It consumes linker-generated initialization tables:

- `__INIT_TABLE`: flash-to-RAM copy ranges.
- `__ZERO_TABLE`: RAM ranges to clear.

It copies initialized data and zeros BSS-like regions before normal C code relies on them.

### `SystemInit()`

Currently empty. It is reserved for later system-level initialization such as clock setup, MPU configuration, cache policy, and security-domain configuration.

## 13. Linker and Memory Considerations

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

The task stacks currently live in the ordinary C data/BSS placement selected by the linker. They are not yet placed in dedicated linker sections. Before MPU enforcement, task stacks should move into explicit sections with symbols for:

- Kernel stack.
- Per-task stack regions.
- Guard gaps or no-access regions.
- Privileged kernel data.
- Unprivileged task data.

## 14. Debugger Test Procedure

Build the image:

```text
make
```

Start the target and add these watch expressions:

```text
g_current_task_index
g_fault_active
g_fault_record.pc
g_fault_record.lr
g_fault_record.cfsr
g_fault_record.hfsr
```

Expected runtime behavior:

- The run LED toggles every 100 ticks.
- Task 1 periodically enters `TASK_SLEEPING` for seven ticks.
- The idle task can run while workers are unavailable.
- No fault handler should be reached during normal operation.

If execution stops in a fault handler:

1. Inspect `g_fault_active`.
2. Read `g_fault_record.fault_type`.
3. Inspect `g_fault_record.pc` and `g_fault_record.lr`.
4. Decode `g_fault_record.cfsr`.
5. Compare `g_fault_record.exc_return` with whether the fault came from MSP or PSP.

## 15. Current Guarantees

The current design provides these useful guarantees:

- Task stacks are statically allocated.
- Task context is switched through PSP.
- Kernel and exception entry use MSP.
- PendSV is lower priority than SysTick.
- Task 1 cannot continue running while its state is sleeping.
- An idle task is available as a scheduler fallback.
- Faults can be inspected after capture instead of immediately losing context.
- The build is freestanding and does not depend on a C runtime or standard library.

## 16. Current Limitations

The following limitations are known and intentional at this stage:

1. The scheduler has three hard-coded task slots.
2. Task stacks are fixed at 512 bytes each.
3. There is no public task creation API.
4. There is no task deletion or termination service.
5. There is no priority scheduler.
6. There is no timeout overflow policy.
7. There is no synchronization primitive.
8. There is no IPC.
9. MPU protection currently covers task-stack guard regions only.
10. Tasks currently execute privileged.
11. Fault handlers do not yet capture the floating-point extended frame.
12. The task scheduler does not yet document every interrupt-context restriction for its shared-data helpers.
13. The SysTick reload value is hard-coded.
14. Watchdog servicing and watchdog recovery are not integrated.
15. Cache maintenance and memory attributes are not yet part of the kernel API.

## 17. Recommended Development Order

The next low-level milestones should be implemented in this order:

### 17.1 Stack validation

Stack watermarking, scheduler-time bounds checks, and MPU no-access guard
regions below each task stack are implemented. The next refinement is to
exercise the guard deliberately under the debugger and verify the captured
MemManage record.

### 17.2 Critical-section primitives

Architecture-specific PRIMASK helpers are present and protect task state, sleep accounting, and scheduler selection. The next refinement is to define which APIs are legal from thread mode, SVC, SysTick, and PendSV context.

### 17.3 Board and RTD boundary

The native board layer is now present for the PTB18 heartbeat. RTD should be introduced when additional production peripheral services are needed, such as clock, pin, GPIO, watchdog, CAN, or ADC configuration. Replace board implementations behind the same interface rather than coupling RTD to kernel code.

### 17.4 Fault hardening

Add fault nesting detection, a reset policy, and persistent fault storage in a reserved RAM or data-flash region.

### 17.4 MPU setup

Guard regions are configured. The remaining MPU work is to define linker
sections for complete kernel and task memory regions before enabling
unprivileged tasks.

### 17.5 Privilege transition

Tasks currently launch privileged. A future privilege transition must first
define complete memory regions and validated SVC services.

### 17.6 Time services

Add a monotonic tick type, timeout comparison helpers, and a defined tick-wrap policy.

### 17.7 IPC

Add one bounded static queue first. Define ownership, blocking behavior, timeout behavior, and ISR restrictions before adding semaphores or event flags.

### 17.8 Watchdog integration

Assign watchdog responsibilities to the kernel and define what happens when a task misses its service window.

## 18. Commit History Context

The implementation evolved through small debugger-tested increments:

- Boot-stage markers and kernel entry.
- Static task model.
- SysTick and PendSV instrumentation.
- PSP-based task stacks.
- First-task launch correction.
- SVC yield and sleep services.
- Idle task and sleeping task states.
- Kernel folder refactor.
- Fault capture diagnostics.

The code should continue to be changed in small increments with a build and debugger check after each step.
