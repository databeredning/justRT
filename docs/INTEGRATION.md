# Integrating justRT into an Application

justRT is included as source in the application firmware. The repository does
not build a standalone library because the kernel, Cortex-M port, application
configuration, startup code, and linker layout must use one consistent set of
target options.

This guide covers the existing Cortex-M port. Start with the repository's
S32K312 or QEMU platform when possible. When an application already has a BSP,
startup file, vector table, and linker script, keep those files and integrate
the justRT requirements described below.

## Application configuration

Copy the repository `JRTConfig.h` into an application-owned configuration
directory and adjust it for the application. Put that directory before the
justRT root in every C compilation command:

```make
APP_CONFIG_DIR := app/config
JUSTRT_DIR := external/justRT

CPPFLAGS += -I$(APP_CONFIG_DIR)
CPPFLAGS += -I$(JUSTRT_DIR)
CPPFLAGS += -I$(JUSTRT_DIR)/kernel
CPPFLAGS += -I$(JUSTRT_DIR)/arch
```

The first matching `JRTConfig.h` is used. All kernel, architecture, and
application objects in an image must use the same header. Rebuild all of them
after changing the configuration or include order.

The application selects task capacity, tick frequency, priorities, stack
sizes, and optional benchmark/test features. The target build selects CPU
capabilities such as the FPU, MPU, and cycle counter.

## Required sources

Add the portable kernel and Cortex-M implementation to the application build:

```make
JUSTRT_SOURCES := \
    $(JUSTRT_DIR)/kernel/task.c \
    $(JUSTRT_DIR)/kernel/sync.c \
    $(JUSTRT_DIR)/kernel/timer.c \
    $(JUSTRT_DIR)/kernel/mempool.c \
    $(JUSTRT_DIR)/kernel/benchmark.c \
    $(JUSTRT_DIR)/kernel/fatal.c \
    $(JUSTRT_DIR)/arch/cortex_m/port_cm7.c \
    $(JUSTRT_DIR)/arch/cortex_m/fault.c \
    $(JUSTRT_DIR)/arch/cortex_m/svc_stubs_cm7.c

JUSTRT_ASM_SOURCES := \
    $(JUSTRT_DIR)/arch/cortex_m/svc_cm7.s
```

`benchmark.c` remains in the source list when benchmarking is disabled; its
implementation compiles out according to `JRT_ENABLE_TASK_BENCHMARK`.

Use the same Cortex-M CPU, Thumb, floating-point ABI, optimization, and
preprocessor options for justRT and application sources. Compile the assembly
file with the same CPU and floating-point ABI options. Required target feature
definitions are:

```make
# Example: Cortex-M7 with FPU, MPU, and DWT cycle counter
CPPFLAGS += -DJRT_ARCH_FPU_CONTEXT=1
CPPFLAGS += -DJRT_ARCH_HAS_MPU=1
CPPFLAGS += -DJRT_ARCH_HAS_DWT_CYCCNT=1
CFLAGS   += -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
ASFLAGS  += -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
```

Set unsupported features to `0`. The FPU setting and compiler floating-point
ABI must agree. `JRT_CORE_CLOCK_HZ` in the application configuration must
describe the clock that is active when `JRT_KernelStart()` configures SysTick.

## Platform support

The platform include path must contain `board/board.h`. It declares:

```c
void board_init(void);
void board_led_toggle(void);
```

Provide corresponding platform implementations. `board_led_toggle()` is the
current example board service reached through an unprivileged SVC. If the
application does not use that API, a no-op implementation is sufficient, but
the symbol is still required by the Cortex-M port.

Call `board_init()` from privileged startup/application code before
`JRT_KernelInit()` and before interrupts or tasks can use board peripherals.
The kernel does not call it automatically.

To reuse a platform supplied by this repository, also compile its `system.c`,
startup assembly, vector table, and `board/board.c`, and link with its linker
script. Do not compile those files when the application already provides the
same responsibilities.

## Startup and exception vectors

An existing BSP may retain its reset handler and normal interrupt vectors. It
must initialize `.data`, clear `.bss` and justRT's zero-initialized sections,
establish a valid main stack, initialize the processor clock, and then call the
application's `main()`.

Connect these Cortex-M exception slots to justRT's implementations:

| Vector | justRT symbol |
| --- | --- |
| HardFault | `HardFault_Handler` |
| MemManage | `MemManage_Handler` |
| BusFault | `BusFault_Handler` |
| UsageFault | `UsageFault_Handler` |
| SVCall | `SVC_Handler` |
| PendSV | `PendSV_Handler` |
| SysTick | `SysTick_Handler` |

justRT owns SVC, PendSV, and SysTick. An application cannot retain another
RTOS handler or an independent SysTick handler under those vector names.
Application interrupts remain in the BSP's vector table and may call only the
ISR-safe APIs documented in the [kernel reference](RTOS.md).

## Linker requirements

The application linker script must retain these input sections:

```text
.privileged_functions
.privileged_exceptions
.privileged_data
.unprivileged_functions
.unprivileged_svc
.unprivileged_rodata
.unprivileged_task_data
.task_private_data
```

It must define these bounds even when a corresponding section is empty:

```text
__unprivileged_functions_start  __unprivileged_functions_end
__unprivileged_svc_start        __unprivileged_svc_end
__unprivileged_rodata_start     __unprivileged_rodata_end
__unprivileged_task_data_start  __unprivileged_task_data_end
__task_private_data_start       __task_private_data_end
```

The private-data bounds are used by kernel validation even on targets with the
MPU disabled. With `JRT_ARCH_HAS_MPU=1`, the remaining unprivileged bounds are
also consumed when the port programs the MPU. Follow the alignment, exact
range, and overlap rules in the [porting guide](PORT.md). The supplied target
linker scripts are the authoritative working examples.

The startup code must copy initialized sections and clear every `NOLOAD`
section that contains justRT state. If the application uses a generic linker
script, verify this explicitly rather than assuming `.bss` includes the custom
unprivileged sections.

## Starting the kernel

Define static task stacks and task definitions, initialize the board, and pass
the task array to the kernel:

```c
#include "kernel.h"
#include "board/board.h"

static JRT_TASK_UNPRIVILEGED void worker(void *argument)
{
    (void)argument;
    for (;;)
    {
        JRT_TaskDelay(10U);
    }
}

JRT_DECLARE_STATIC_TASK_STACK(worker_stack, JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t tasks[] JRT_TASK_UNPRIVILEGED_RODATA = {
    JRT_TASK_DEFINITION(worker, 0U, worker_stack, 1U, "worker",
                        JRT_TASK_FLAG_UNPRIVILEGED)
};

int main(void)
{
    const JRT_KernelConfig_t config = {
        tasks,
        sizeof(tasks) / sizeof(tasks[0])
    };

    board_init();
    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        return 1;
    }
    JRT_KernelStart();
}
```

`JRT_KernelStart()` does not return. Call it from privileged thread mode with
maskable interrupts in the state expected by the platform. It configures the
tick and enters the first task through SVC.

The repository [simple example](../examples/simple.c) shows task definition
and startup without product-specific initialization. The root `Makefile`
shows the complete source and target flag selection for both supported
platforms.

## Integration checklist

- One application-owned `JRTConfig.h` is selected for every C source.
- CPU/FPU ABI options match across C, assembly, and link commands.
- Exactly one implementation owns SVC, PendSV, SysTick, and CPU fault vectors.
- The linker retains all justRT sections and exports the required bounds.
- Startup initializes all data and zero-initialized sections.
- `board/board.h`, `board_init()`, and `board_led_toggle()` are provided.
- Board and clock initialization complete before `JRT_KernelStart()`.
- The final image is tested in its optimized build, including FPU and MPU
  behavior when those features are enabled.
