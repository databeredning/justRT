# PORT.md — Porting JustBoot to a New Architecture or Platform

This document explains the port boundary established by the architecture
extraction (see `RTOS.md` chapter 16), what exactly must be implemented to
bring the kernel up on a new CPU family or board, and walks through a
concrete worked example: porting to a Cortex-M3 target running under QEMU
(`qemu-system-arm -M mps2-an385`).

## 1. The three layers

```
kernel/            portable kernel: scheduler, sync primitives, timers,
                   memory pools. No CPU register access. No board access.
arch/<family>/     CPU-family port. Implements the port contract
                   (arch/<family>/port_contract.h) plus the exception
                   vectors that are reached only via the vector table.
platform/<board>/  Startup code, vector table, linker script, and board
                   (LED/GPIO/UART) driver for one specific chip/board.
```

Only `platform/` should know about a specific chip or board. Only `arch/`
should know about the CPU family's registers (NVIC, SysTick, MPU, PRIMASK,
CONTROL). `kernel/` should know about neither — it only calls the contract
functions declared in `arch/<family>/port_contract.h`.

To port JustBoot to a new target you generally need:

- A new `arch/<family>/` **only if the CPU family changes** (different
  instruction set family, e.g. RISC-V, or a Cortex-M without MPU/FPU that
  needs a reduced contract implementation). Porting to another Cortex-M3/M4/M7
  chip can usually reuse `arch/cortex_m/` unchanged.
- Always a new `platform/<board>/` for a new chip or board.
- Makefile changes to point at the new arch/platform directories.

## 2. The port contract (`arch/cortex_m/port_contract.h`)

The kernel calls exactly these functions from `kernel/task.c`, `sync.c`,
`timer.c`, and `mempool.c`. Implement every one of them for a new arch.

| Function | Called from | Must do |
| --- | --- | --- |
| `void arch_request_switch(void)` | `task.c`, `sync.c` after waking a task | Pend a context switch so it happens at the next opportunity (lowest exception priority). On Cortex-M this is `PendSV`. |
| `uint32_t arch_critical_enter(void)` | everywhere kernel state is touched | Disable interrupts (or raise to a priority above all kernel-using ISRs) and return an opaque token that restores the prior state. |
| `void arch_critical_exit(uint32_t saved)` | paired with the above | Restore interrupts to the state captured by `arch_critical_enter()`. Must nest correctly (save/restore, not a plain enable). |
| `int arch_in_isr(void)` | `sync.c` ISR-vs-task API guards, `task.c` event-group ISR path | Return non-zero only when the CPU is currently executing in exception/interrupt context. |
| `void arch_tick_init(void)` | `task.c` `kernel_start()` | Configure and start the periodic tick interrupt at the tick rate in `kernel.h` (`KERNEL_TICK_RATE_HZ`), and set exception priorities so the tick and switch exceptions are the two lowest-priority exceptions in the system. |
| `void arch_yield(void)` | `task.c` every blocking wait | Force an immediate reschedule and return only after the calling task is scheduled again. On Cortex-M this is an `SVC` instruction handled by `svc_dispatch()`. |
| `void arch_configure_mpu(void *const *guard_addresses, uint32_t guard_count)` | `task.c` `kernel_init()` | Program a not-a-must memory-protection scheme: flash/SRAM base regions plus one no-access guard region per task stack (`guard_addresses[i]`, each `KERNEL_TASK_GUARD_WORDS * 4` bytes). If the target has no MPU, this can be a no-op — stack guards then rely on `update_stack_usage()`'s software bounds check alone (see `RTOS.md` chapter 8). |
| `void arch_start_first_task(uint32_t *sp, uint32_t control_value)` | `task.c` `kernel_start()`, once | Never returns. Restore the first task's saved register frame from `sp` (the layout is defined by `build_initial_stack()` in `task.c` — 8 hardware-stacked words followed by 8 software-stacked words r4-r11) and drop to the requested privilege/stack mode. `control_value` is either `ARCH_LAUNCH_PRIVILEGED` or `ARCH_LAUNCH_UNPRIVILEGED` from the contract header. |
| `void arch_wait_for_interrupt(void)` | `task.c` idle task | Put the CPU in its lowest-overhead wait state until the next interrupt (Cortex-M `wfi`; a busy-loop is also legal but wastes power). |

### Constants the contract also defines

- `ARCH_MPU_GUARD_REGION_COUNT` — maximum number of tasks `arch_configure_mpu()`
  can guard. `kernel_init()` rejects configurations that would exceed it.
- `ARCH_LAUNCH_PRIVILEGED` / `ARCH_LAUNCH_UNPRIVILEGED` — values passed to
  `arch_start_first_task()`. Their meaning is arch-defined; on Cortex-M they
  are CONTROL register values (2 and 3).

### Exception vectors that are *not* part of the contract

`SysTick_Handler`, `PendSV_Handler`, `SVC_Handler`, `HardFault_Handler`,
`MemManage_Handler`, `BusFault_Handler`, and `UsageFault_Handler` are reached
only through the vector table, never called by name from the portable
kernel. They still must exist in `arch/<family>/`, but they are not part of
the contract header — they are the arch's own private implementation detail.
On Cortex-M:

- `SysTick_Handler()` calls `tick_tasks()` (portable), then the registered
  tick hook (`kernel_set_tick_hook()`), then `arch_request_switch()`.
- `PendSV_Handler()` is a naked handler that saves r4-r11, calls
  `pendsv_switch(sp)` (portable — returns the next task's saved sp), restores
  r4-r11, and returns.
- `SVC_Handler` (in `arch/cortex_m/svc_cm7.s`) picks MSP or PSP based on
  `EXC_RETURN` bit 2, then calls `svc_dispatch(stacked_frame, exc_return)`
  (arch-owned, in `port_cm7.c`), which validates the caller returned to
  Thread mode and dispatches `SVC_SERVICE_YIELD` / `SVC_SERVICE_SLEEP` /
  `SVC_SERVICE_LED_TOGGLE`.
- The four fault handlers capture the exception frame and fault status
  registers into `g_fault_record` (see `kernel/fault.c`) and spin forever;
  they have no portable-kernel call sites at all.

## 3. MPU / memory-protection contract (if implemented)

`arch_configure_mpu()` is expected to set up, at minimum:

1. A general flash (code) region.
2. A general SRAM region.
3. One no-access guard region per task, placed at `guard_addresses[i]`
   (`KERNEL_TASK_GUARD_WORDS * 4` = 32 bytes on this port), used to catch
   stack overflow via `MemManage_Handler`.
4. If unprivileged tasks are used: an unprivileged-writable data region
   covering the linker's `.unprivileged_task_data` output section.

Region numbering matters: guard regions must have a **higher** priority
(higher region number on Cortex-M, where higher numbers win overlaps) than
the general SRAM region, or a stack guard will be silently shadowed.

If the new target has no MPU, `arch_configure_mpu()` can be an empty
function — `kernel_init()` will still call it, and stack safety continues to
rely on `update_stack_usage()`'s software check (which halts and records
`g_stack_fault`/`g_stack_fault_task`/`g_stack_fault_sp` regardless of MPU
presence).

## 4. Linker script contract

The linker script is entirely platform-owned, but the kernel/arch code
expects certain **sections** and **symbols** to exist, because attributes in
`kernel/kernel.h` and `board/board.h`-equivalent headers place code/data into
named sections:

| Section macro (from `kernel.h`) | Linker section | Purpose |
| --- | --- | --- |
| `KERNEL_PRIVILEGED` | `.privileged_functions` | Kernel + board code that must run privileged. |
| `KERNEL_PRIVILEGED_DATA` | `.privileged_data` | Kernel state (task table, counters). |
| `TASK_UNPRIVILEGED` | `.unprivileged_functions` | Application task code allowed to run unprivileged. |
| `TASK_UNPRIVILEGED_RODATA` | `.unprivileged_rodata` | Read-only task tables read from unprivileged code. |
| `TASK_UNPRIVILEGED_DATA` | `.unprivileged_task_data` | Task-owned RAM, covered by the MPU unprivileged-data region. |
| (SVC trampoline macro in `port_contract.h`/arch) | `.system_calls` | The `svc` instruction wrapper only, so it stays reachable from unprivileged code. |

**Ordering matters**: on this port, `.privileged_functions` is placed at a
*higher* MPU region number than the general flash region so its (currently
identical) permissions don't get shadowed; if you introduce
read-only/execute-only splits, keep narrow/more-specific regions at higher
region numbers. See `RTOS.md` chapter 16 and the note in the linker script
about section ordering.

Symbols the arch/platform startup code is expected to define (see
`platform/s32k312/linker_flash_s32k312.ld` for the full worked example):

- `__unprivileged_task_data_start` / `_end` — consumed directly by
  `arch_configure_mpu()` in `port_cm7.c`.
- Whatever copy/zero-table symbols your startup's `init_data_bss()`
  equivalent needs (S32K312 uses `__init_table`/`__zero_table`, generated in
  `startup_cm7.s` and consumed by `system.c`). A QEMU or other bare-metal
  target can normally use the compiler's default `.data`/`.bss` handling
  instead and skip this table entirely (see the worked example below).

## 5. Board interface contract

Application examples call a tiny, project-defined board interface (not part
of `arch/`, lives under `platform/<board>/board/`):

```c
void board_init(void);       /* one-time GPIO/UART/etc. setup */
void board_led_toggle(void); /* toggle whatever the examples use as "the LED" */
```

Both are marked `BOARD_PRIVILEGED` (same section as `KERNEL_PRIVILEGED`) so
they remain callable from the SVC gateway (`SVC_SERVICE_LED_TOGGLE` in
`svc_dispatch()`) even when the calling task is unprivileged.

## 6. Build system integration

The Makefile needs, for a new arch/platform pair:

- `-I<path-to-arch-root>` (currently `-Iarch`) so `#include "cortex_m/port_contract.h"`-style includes resolve.
- `-I<path-to-platform-root>` (currently `-Iplatform/s32k312`) so
  `#include "board/board.h"` resolves to the new board driver.
- New object rules for the platform's startup/vector-table/system-init
  sources and the board driver.
- `-T <path-to-new-linker-script>` in `LDFLAGS`.
- CPU flags (`-mcpu=...`, `-mfpu=...`, `-mfloat-abi=...`) matching the new
  target; drop `-mfpu`/`-mfloat-abi` entirely for FPU-less cores (Cortex-M3/M0).

## 7. Worked example: porting to QEMU `mps2-an385` (Cortex-M3)

This target is a good first port to try because:

- It is Cortex-M3 (ARMv7-M, same NVIC/SysTick/PendSV/SVC/PRIMASK model as the
  existing `arch/cortex_m`), so **`arch/cortex_m/port_contract.h`,
  `port_cm7.c`, `svc_cm7.s`, and `fault.c` can be reused almost unchanged** —
  only the MPU code needs adjusting (Cortex-M3 either lacks an MPU or has a
  smaller region count depending on variant; if absent, stub
  `arch_configure_mpu()` per section 3) and the FPU-related build flags must
  be dropped (Cortex-M3 has no FPU).
- QEMU boots straight into the vector table with SRAM/flash already mapped
  and clocked — there is no NXP-style MC_ME clock-gating/PLL sequence to
  reimplement, so the new `platform/qemu_mps2an385/startup.s` is a fraction
  of the size of `platform/s32k312/startup_cm7.s`.
- It has a real, well-known memory map and a CMSDK UART you can use in place
  of the LED for `board_led_toggle()` (toggle a GPIO bit if you want visual
  parity, or write a byte to the UART as a simpler stand-in).

### 7.1 Decide the arch reuse strategy

Create `arch/cortex_m3/` only if you need a *reduced* contract (no MPU, no
FPU-related concerns). Otherwise, prefer adding a compile-time guard to the
existing `arch/cortex_m/port_cm7.c` (e.g. `#if defined(ARCH_HAS_MPU)`
around the MPU register defines and `arch_configure_mpu()` body, falling
back to a no-op) rather than forking the whole file. This keeps one arch
implementation serving multiple Cortex-M variants, which is the model this
project already uses for privileged/unprivileged task flags.

### 7.2 Create `platform/qemu_mps2an385/`

```
platform/qemu_mps2an385/
  startup.s                 -- reset handler, minimal stack setup, jumps to main()
  Vector_Table.s             -- same layout as platform/s32k312/Vector_Table.s
                               (Stack pointer, Reset, NMI, HardFault, MemManage,
                               BusFault, UsageFault, ..., SVC, PendSV, SysTick)
  linker_qemu_mps2an385.ld   -- memory map for mps2-an385 (flash at 0x00000000,
                               SRAM at 0x20000000 are the QEMU/MPS2 defaults;
                               confirm against your QEMU version's -M help output)
  board/board.c              -- board_init()/board_led_toggle() using the
                               CMSDK GPIO or UART peripheral at its documented
                               base address for this machine
  board/board.h
```

Minimal `startup.s` (no clock tree, no ECC/TCM init needed under QEMU):

```asm
.syntax unified
.thumb
.section .vectors, "ax"
.section .text.Reset_Handler, "ax"
.thumb_func
.global Reset_Handler
Reset_Handler:
    ldr r0, =__data_load_start
    ldr r1, =__data_start
    ldr r2, =__data_end
copy_data:
    cmp r1, r2
    beq zero_bss
    ldr r3, [r0], #4
    str r3, [r1], #4
    b copy_data
zero_bss:
    ldr r1, =__bss_start
    ldr r2, =__bss_end
    movs r3, #0
zero_loop:
    cmp r1, r2
    beq call_main
    str r3, [r1], #4
    b zero_loop
call_main:
    bl main
hang:
    b hang
```

This is deliberately simpler than `platform/s32k312/startup_cm7.s`: standard
`.data`/`.bss` copy/zero loops instead of the S32K312's `.init_table`/
`.zero_table` scheme, because QEMU needs none of the MC_ME/PLL/ECC bring-up
this chip requires.

Reuse `platform/s32k312/Vector_Table.s` almost verbatim — only the initial
stack pointer symbol name and the `.section` name may need to match your new
linker script.

### 7.3 Minimal linker script sketch

```ld
ENTRY(Reset_Handler)
MEMORY
{
    flash (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00040000
    sram  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00010000
}
SECTIONS
{
    .vectors : { KEEP(*(.vectors)) } > flash
    .privileged_functions : { *(.privileged_functions .privileged_functions.*) } > flash
    .unprivileged_functions : { *(.unprivileged_functions .unprivileged_functions.*) } > flash
    .unprivileged_rodata : { *(.unprivileged_rodata .unprivileged_rodata.*) } > flash
    .system_calls : { *(.system_calls .system_calls.*) } > flash
    .text : { *(.text .text.*) *(.rodata .rodata.*) } > flash
    __data_load_start = LOADADDR(.data);
    .privileged_data : { *(.privileged_data .privileged_data.*) } > sram AT> flash
    .unprivileged_task_data (NOLOAD) :
    {
        __unprivileged_task_data_start = .;
        *(.unprivileged_task_data .unprivileged_task_data.*)
        __unprivileged_task_data_end = .;
    } > sram
    .data : { __data_start = .; *(.data .data.*); __data_end = .; } > sram AT> flash
    .bss (NOLOAD) : { __bss_start = .; *(.bss .bss.*) *(COMMON); __bss_end = .; } > sram
    . = ALIGN(8);
    __StackTop = ORIGIN(sram) + LENGTH(sram);
}
```

Adjust the `ORIGIN`/`LENGTH` values to match whatever `-M mps2-an385 -m ...`
reports for your QEMU version; use `qemu-system-arm -M mps2-an385 -kernel
build/test.elf -S -s` with GDB attached to confirm the reset vector and
memory map if unsure.

### 7.4 Board driver stand-in

```c
/* platform/qemu_mps2an385/board/board.c */
#include "board.h"

#define CMSDK_UART0_BASE 0x40004000U
#define UART_DATA (*(volatile uint32_t *)(CMSDK_UART0_BASE + 0x00))
#define UART_STATE (*(volatile uint32_t *)(CMSDK_UART0_BASE + 0x04))
#define UART_CTRL (*(volatile uint32_t *)(CMSDK_UART0_BASE + 0x08))
#define UART_CTRL_TX_ENABLE (1UL << 0)

static uint32_t led_state;

void board_init(void)
{
    UART_CTRL = UART_CTRL_TX_ENABLE;
}

void board_led_toggle(void)
{
    led_state ^= 1U;
    UART_DATA = led_state ? 'X' : '.';
}
```

Replace this with real GPIO register writes if your QEMU machine models a
GPIO block you want to exercise instead.

### 7.5 Makefile changes

```make
PLATFORM_DIR := platform/qemu_mps2an385
CPUFLAGS := -mcpu=cortex-m3 -mthumb            # no -mfpu/-mfloat-abi: no FPU
CFLAGS += -I$(PLATFORM_DIR)
LDFLAGS += -T $(PLATFORM_DIR)/linker_qemu_mps2an385.ld
OBJS += $(OBJDIR)/startup.o $(OBJDIR)/Vector_Table.o $(OBJDIR)/board/board.o
```

(Follow the same object-rule pattern already used for
`platform/s32k312/*` in the existing `Makefile`.)

### 7.6 Running under QEMU

```sh
make -B MAIN_PROFILE=0
qemu-system-arm -M mps2-an385 -nographic -kernel bin/justboot.elf
```

Use `-s -S` to pause at reset and attach GDB (`target remote :1234`) the same
way the existing S32K312 `gdb-server` workflow does, substituting the QEMU
gdbstub for the physical debug probe.

### 7.7 Validation checklist for any new port

Once the new arch/platform builds:

1. Profile 0 — confirms `arch_tick_init()`, `arch_start_first_task()`
   (privileged path), `arch_request_switch()`, and the scheduler run at all.
2. Profile 6 (`heartbeat_unprivileged_led_start()`) — confirms the
   unprivileged `arch_start_first_task()` path and the SVC gateway
   (`board_led_toggle()` called through `SVC_SERVICE_LED_TOGGLE`), *only if*
   your target supports privilege levels/MPU; skip if not applicable.
3. Profile 1, 2, or 3 — confirms `arch_in_isr()` and ISR-context give/send
   APIs work from a real interrupt (SysTick tick hook on this port).
4. Profile 13 — confirms `arch_configure_mpu()` guard-region placement for
   more than two tasks, if MPU is implemented.
5. `g_stack_fault` stays `0` and `g_context_switches` increases steadily
   across all of the above — this is the same acceptance bar used throughout
   the original architecture extraction (see the commit history for
   `arch: extract ...` and the paired `validate: confirm ...` commits).
