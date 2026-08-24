#ifndef JUSTBOOT_ARCH_CORTEX_M_PORT_CONTRACT_H
#define JUSTBOOT_ARCH_CORTEX_M_PORT_CONTRACT_H

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
 */

void arch_request_switch(void);

#endif
