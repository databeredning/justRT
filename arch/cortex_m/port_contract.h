#ifndef JUSTRT_ARCH_CORTEX_M_PORT_CONTRACT_H
#define JUSTRT_ARCH_CORTEX_M_PORT_CONTRACT_H

#include <stdint.h>

#include "JRTConfig.h"

/*
 * Cortex-M port contract.
 *
 * This header names the boundary between the portable kernel (kernel/task.c,
 * kernel/sync.c, kernel/timer.c, kernel/mempool.c) and the Cortex-M-specific
 * port in arch/cortex_m/ (port_cm7.c, svc_cm7.s, and fault.c).
 * Functions are added here incrementally as each piece of port behavior is
 * extracted; until a function is listed, the kernel still calls the port's
 * native name directly.
 *
 * Extracted so far:
 *   - arch_build_initial_stack(): construct the initial task context.
 *   - arch_disable_interrupts()/arch_halt(): fatal-path CPU operations.
 *   - arch_request_switch(): pend a context switch (PendSV).
 *   - arch_critical_enter()/arch_critical_exit(): PRIMASK-based critical
 *     sections.
 *   - arch_in_isr(): true when called from exception/interrupt context.
 *   - arch_tick_init()/arch_tick_start(): configure and start the tick.
 *   - arch_yield(): request an immediate reschedule via SVC.
 *   - arch_configure_mpu(): program the static MPU map and the first task's
 *     dynamic stack guard and private-data region.
 *   - arch_set_task_stack_guard(): replace the dynamic guard for the task
 *     selected by the scheduler.
 *   - arch_set_task_private_data(): replace or disable the dynamic private
 *     data region for the task selected by the scheduler.
 *   - arch_start_first_task(): drop to the first task's stack/privilege
 *     level and never return.
 *   - arch_wait_for_interrupt(): idle until the next interrupt (`wfi`).
 *
 * SysTick_Handler(), PendSV_Handler() and SVC_Handler() are exception
 * vectors and stay named by the vector table (Vector_Table.s); they are
 * already fully owned by the port and are not renamed here.
 *
 * arch/cortex_m/fault.c (HardFault/MemManage/BusFault/UsageFault handlers and
 * fault_capture()) has no call sites from the portable kernel at all -- it
 * is reached only via the vector table -- so it needs no seam here. It is
 * already fully arch-owned.
 */

/* CONTROL register value requesting privileged Thread-mode execution. */
#define ARCH_LAUNCH_PRIVILEGED 2U
/* CONTROL register value requesting unprivileged Thread-mode execution. */
#define ARCH_LAUNCH_UNPRIVILEGED 3U

/* Return to Thread mode using PSP with a basic (non-FP) hardware frame. */
#define ARCH_INITIAL_EXC_RETURN 0xFFFFFFFDU

/* Cortex-M basic and optional floating-point context sizes, in words. */
#define JRT_INITIAL_STACK_USED_WORDS 17U
#define JRT_FP_SOFTWARE_CONTEXT_WORDS 16U
#define JRT_FP_HARDWARE_CONTEXT_WORDS 18U
#if JRT_ARCH_FPU_CONTEXT
#define JRT_MINIMUM_TASK_STACK_WORDS \
	(JRT_INITIAL_STACK_USED_WORDS + JRT_FP_SOFTWARE_CONTEXT_WORDS \
	 + JRT_FP_HARDWARE_CONTEXT_WORDS)
#else
#define JRT_MINIMUM_TASK_STACK_WORDS JRT_INITIAL_STACK_USED_WORDS
#endif

/* Construct the synthetic exception frame consumed by context restore. */
uint32_t *arch_build_initial_stack(uint32_t *stack_top, void (*entry)(void *), void *argument);
/* Fatal-path operations: no critical-section accounting or mask restoration. */
void arch_disable_interrupts(void);
void arch_halt(void) __attribute__((noreturn));

void arch_request_switch(void);
uint32_t arch_critical_enter(void);
void arch_critical_exit(uint32_t saved_primask);
int arch_in_isr(void);
void arch_tick_init(void);
void arch_tick_start(void);
void arch_yield(void);
void arch_configure_mpu(void *guard_address, void *private_data_base, uint32_t private_data_size);
void arch_set_task_stack_guard(void *guard_address);
void arch_set_task_private_data(void *base, uint32_t size);
void arch_start_first_task(void);
void arch_wait_for_interrupt(void);
#if JRT_ENABLE_TASK_BENCHMARK
void arch_cycle_counter_init(void);
int arch_cycle_counter_available(void);
uint32_t arch_cycle_counter_read(void);
#endif

#endif
