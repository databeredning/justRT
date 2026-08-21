#include <stdint.h>

#include "kernel.h"

#define SCB_CFSR (*(volatile uint32_t *)0xE000ED28U)
#define SCB_ICSR (*(volatile uint32_t *)0xE000ED04U)
#define SCB_CCR (*(volatile uint32_t *)0xE000ED14U)
#define SCB_SHCSR (*(volatile uint32_t *)0xE000ED24U)
#define SCB_HFSR (*(volatile uint32_t *)0xE000ED2CU)
#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0CU)
#define SCB_DFSR (*(volatile uint32_t *)0xE000ED30U)
#define SCB_MMFAR (*(volatile uint32_t *)0xE000ED34U)
#define SCB_BFAR (*(volatile uint32_t *)0xE000ED38U)
#define SCB_AFSR (*(volatile uint32_t *)0xE000ED3CU)

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

volatile fault_record_t g_fault_record KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_fault_active KERNEL_PRIVILEGED_DATA = 0U;
volatile svc_diagnostic_t g_svc_diagnostic KERNEL_PRIVILEGED_DATA;

void svc_capture(uint32_t *stacked_frame, uint32_t exc_return)
{
    uint32_t value;

    __asm volatile ("mrs %0, ipsr" : "=r" (value) : : "memory");
    g_svc_diagnostic.live_ipsr = value;
    g_svc_diagnostic.live_icsr = SCB_ICSR;
    __asm volatile ("mrs %0, control" : "=r" (value) : : "memory");
    g_svc_diagnostic.live_control = value;
    __asm volatile ("mrs %0, psp" : "=r" (value) : : "memory");
    g_svc_diagnostic.live_psp = value;
    __asm volatile ("mrs %0, msp" : "=r" (value) : : "memory");
    g_svc_diagnostic.live_msp = value;
    __asm volatile ("mrs %0, primask" : "=r" (value) : : "memory");
    g_svc_diagnostic.live_primask = value;
    __asm volatile ("mrs %0, basepri" : "=r" (value) : : "memory");
    g_svc_diagnostic.live_basepri = value;
    __asm volatile ("mrs %0, faultmask" : "=r" (value) : : "memory");
    g_svc_diagnostic.live_faultmask = value;
    g_svc_diagnostic.live_ccr = SCB_CCR;
    g_svc_diagnostic.live_shcsr = SCB_SHCSR;
    g_svc_diagnostic.live_aircr = SCB_AIRCR;
    g_svc_diagnostic.exc_return = exc_return;
    g_svc_diagnostic.stacked_r0 = stacked_frame[0];
    g_svc_diagnostic.stacked_r1 = stacked_frame[1];
    g_svc_diagnostic.stacked_r2 = stacked_frame[2];
    g_svc_diagnostic.stacked_r3 = stacked_frame[3];
    g_svc_diagnostic.stacked_r12 = stacked_frame[4];
    g_svc_diagnostic.stacked_lr = stacked_frame[5];
    g_svc_diagnostic.stacked_pc = stacked_frame[6];
    g_svc_diagnostic.stacked_xpsr = stacked_frame[7];
}

void fault_capture(uint32_t *stacked_frame, uint32_t exc_return, uint32_t fault_type)
{
    g_fault_record.r0 = stacked_frame[0];
    g_fault_record.r1 = stacked_frame[1];
    g_fault_record.r2 = stacked_frame[2];
    g_fault_record.r3 = stacked_frame[3];
    g_fault_record.r12 = stacked_frame[4];
    g_fault_record.lr = stacked_frame[5];
    g_fault_record.pc = stacked_frame[6];
    g_fault_record.xpsr = stacked_frame[7];
    g_fault_record.exc_return = exc_return;
    g_fault_record.cfsr = SCB_CFSR;
    g_fault_record.hfsr = SCB_HFSR;
    g_fault_record.dfsr = SCB_DFSR;
    g_fault_record.mmfar = SCB_MMFAR;
    g_fault_record.bfar = SCB_BFAR;
    g_fault_record.afsr = SCB_AFSR;
    g_fault_record.fault_type = fault_type;
    g_fault_active = 1U;

    while (1)
    {
    }
}

#define DEFINE_FAULT_HANDLER(name, type) \
    void name(void) __attribute__((naked)); \
    void name(void) \
    { \
        __asm volatile ( \
            "tst lr, #4\n" \
            "ite eq\n" \
            "mrseq r0, msp\n" \
            "mrsne r0, psp\n" \
            "mov r1, lr\n" \
            "mov r2, %0\n" \
            "b fault_capture\n" \
            : : "I" (type) : "memory"); \
    }

DEFINE_FAULT_HANDLER(HardFault_Handler, 1U)
DEFINE_FAULT_HANDLER(MemManage_Handler, 2U)
DEFINE_FAULT_HANDLER(BusFault_Handler, 3U)
DEFINE_FAULT_HANDLER(UsageFault_Handler, 4U)
