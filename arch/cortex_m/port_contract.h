#ifndef JUSTBOOT_ARCH_CORTEX_M_PORT_CONTRACT_H
#define JUSTBOOT_ARCH_CORTEX_M_PORT_CONTRACT_H

#include <stdint.h>

/*
 * Cortex-M port contract.
 *
 * This header names the boundary between the portable kernel (kernel/task.c,
 * kernel/sync.c, kernel/timer.c, kernel/mempool.c) and the Cortex-M-specific
 * port (currently kernel/port_cm7.c, kernel/svc_cm7.s). Functions are added
 * here incrementally as each piece of port behavior is extracted; until a
 * function is listed, the kernel still calls the port's native name directly.
 *
 * Extracted so far:
 *   - arch_request_switch(): pend a context switch (PendSV).
 *   - arch_critical_enter()/arch_critical_exit(): PRIMASK-based critical
 *     sections.
 *   - arch_in_isr(): true when called from exception/interrupt context.
 */

void arch_request_switch(void);
uint32_t arch_critical_enter(void);
void arch_critical_exit(uint32_t saved_primask);
int arch_in_isr(void);

#endif
