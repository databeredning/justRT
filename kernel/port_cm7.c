#include <stdint.h>

#include "kernel.h"
#include "board/board.h"
#include "cortex_m/port_contract.h"

volatile uint32_t g_svc_invalid_service KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_svc_invalid_context KERNEL_PRIVILEGED_DATA;

#if JRT_ENABLE_TEST_HOOKS
static JRT_KernelTickHook_t tick_hook KERNEL_PRIVILEGED_DATA;
#endif

void JRT_KernelSetTickHook(JRT_KernelTickHook_t hook)
{
#if JRT_ENABLE_TEST_HOOKS
    uint32_t saved_primask = critical_enter();

    tick_hook = hook;
    critical_exit(saved_primask);
#else
    (void)hook;
#endif
}

#define SYST_CSR (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018U)
#define SCB_ICSR (*(volatile uint32_t *)0xE000ED04U)
#define SCB_SHPR3 (*(volatile uint32_t *)0xE000ED20U)
#if JRT_ARCH_FPU_CONTEXT
#define SCB_CPACR (*(volatile uint32_t *)0xE000ED88U)
#define FPU_FPCCR (*(volatile uint32_t *)0xE000EF34U)
#endif

#define SYST_CSR_ENABLE (1UL << 0)
#define SYST_CSR_TICKINT (1UL << 1)
#define SYST_CSR_CLKSOURCE (1UL << 2)
#define SCB_ICSR_PENDSVSET (1UL << 28)
#define SCB_SHPR3_PENDSV_SHIFT 16U
#define SCB_SHPR3_SYSTICK_SHIFT 24U
#if JRT_ARCH_FPU_CONTEXT
#define SCB_CPACR_CP10_CP11_FULL_ACCESS (0xFUL << 20U)
#define FPU_FPCCR_LSPEN (1UL << 30U)
#define FPU_FPCCR_ASPEN (1UL << 31U)
#endif
#define CORTEXM_PRIORITY_BITS 4U
#define PENDSV_LOGICAL_PRIORITY 0x0FU
#define SYSTICK_LOGICAL_PRIORITY 0x0EU
#define CORTEXM_PRIORITY_VALUE(priority) \
    ((priority) << (8U - CORTEXM_PRIORITY_BITS))

enum
{
    SVC_SERVICE_START_FIRST_TASK = 0U,
    SVC_SERVICE_YIELD = 1U,
    SVC_SERVICE_SLEEP = 2U,
    SVC_SERVICE_LED_TOGGLE = 3U,
    SVC_SERVICE_TASK_SUSPEND = 4U,
    SVC_SERVICE_TASK_RESUME = 5U
};

#if JRT_ARCH_HAS_MPU
#define MPU_CTRL (*(volatile uint32_t *)0xE000ED94U)
#define MPU_RNR (*(volatile uint32_t *)0xE000ED98U)
#define MPU_RBAR (*(volatile uint32_t *)0xE000ED9CU)
#define MPU_RASR (*(volatile uint32_t *)0xE000EDA0U)
#define SCB_SHCSR (*(volatile uint32_t *)0xE000ED24U)

#define MPU_CTRL_ENABLE (1UL << 0)
#define MPU_CTRL_PRIVDEFENA (1UL << 2)
#define MPU_RASR_ENABLE (1UL << 0)
#define MPU_RASR_XN (1UL << 28)
#define MPU_RASR_SIZE_32_BYTES (4UL << 1)
#define MPU_RASR_AP_PRIV_RW_UNPRIV_NONE (1UL << 24)
#define MPU_RASR_AP_PRIV_RO_UNPRIV_NONE (5UL << 24)
#define MPU_RASR_AP_READ_WRITE_BOTH (3UL << 24)
#define MPU_RASR_AP_READ_ONLY_BOTH (6UL << 24)
#define MPU_RASR_TEX_NORMAL (1UL << 19)
#define MPU_RASR_CACHEABLE (1UL << 17)
#define MPU_RASR_BUFFERABLE (1UL << 16)
#define SCB_SHCSR_MEMFAULTENA (1UL << 16)
#define MPU_REGION_COUNT 16U

#define MPU_FLASH_REGION 0U
#define MPU_SRAM_REGION 1U
#define MPU_UNPRIVILEGED_FUNCTIONS_REGION 2U
#define MPU_UNPRIVILEGED_SVC_REGION 3U
#define MPU_UNPRIVILEGED_RODATA_REGION 4U
#define MPU_UNPRIVILEGED_DATA_REGION 5U
#define MPU_TASK_PRIVATE_DATA_REGION 14U
#define MPU_STACK_GUARD_REGION 15U

extern uint8_t __unprivileged_functions_start[];
extern uint8_t __unprivileged_functions_end[];
extern uint8_t __unprivileged_svc_start[];
extern uint8_t __unprivileged_svc_end[];
extern uint8_t __unprivileged_rodata_start[];
extern uint8_t __unprivileged_rodata_end[];
extern uint8_t __unprivileged_task_data_start[];
extern uint8_t __unprivileged_task_data_end[];

static void configure_region_range(uint32_t region, uintptr_t start, uintptr_t end, uint32_t attributes)
{
    uintptr_t base;
    uintptr_t size = 32U;
    uint32_t size_encoding = 4U;

    if (end <= start)
    {
        MPU_RNR = region;
        MPU_RASR = 0U;
        return;
    }

    while (size < (end - start))
    {
        size <<= 1U;
        size_encoding++;
    }
    base = start & ~(size - 1U);
    while ((base + size) < end)
    {
        size <<= 1U;
        size_encoding++;
        base = start & ~(size - 1U);
    }

    MPU_RNR = region;
    MPU_RBAR = (uint32_t)base;
    MPU_RASR = attributes | ((size_encoding) << 1U) | MPU_RASR_ENABLE;
}

static void configure_memory_regions(void)
{
    const uint32_t privileged_flash_attributes =
        MPU_RASR_AP_PRIV_RO_UNPRIV_NONE | MPU_RASR_CACHEABLE;
    const uint32_t privileged_sram_attributes =
        MPU_RASR_AP_PRIV_RW_UNPRIV_NONE
        | MPU_RASR_XN | MPU_RASR_TEX_NORMAL | MPU_RASR_CACHEABLE
        | MPU_RASR_BUFFERABLE;
    const uint32_t unprivileged_code_attributes =
        MPU_RASR_AP_READ_ONLY_BOTH | MPU_RASR_CACHEABLE;
    const uint32_t unprivileged_data_attributes =
        MPU_RASR_AP_READ_WRITE_BOTH
        | MPU_RASR_XN | MPU_RASR_TEX_NORMAL | MPU_RASR_CACHEABLE
        | MPU_RASR_BUFFERABLE;
    configure_region_range(MPU_FLASH_REGION, 0x00400000U, 0x00600000U,
                           privileged_flash_attributes);
    configure_region_range(MPU_SRAM_REGION, 0x20400000U, 0x20420000U,
                           privileged_sram_attributes);
    configure_region_range(MPU_UNPRIVILEGED_FUNCTIONS_REGION,
                           (uintptr_t)__unprivileged_functions_start,
                           (uintptr_t)__unprivileged_functions_end,
                           unprivileged_code_attributes);
    configure_region_range(MPU_UNPRIVILEGED_SVC_REGION,
                           (uintptr_t)__unprivileged_svc_start,
                           (uintptr_t)__unprivileged_svc_end,
                           unprivileged_code_attributes);
    configure_region_range(MPU_UNPRIVILEGED_RODATA_REGION,
                           (uintptr_t)__unprivileged_rodata_start,
                           (uintptr_t)__unprivileged_rodata_end,
                           MPU_RASR_AP_READ_ONLY_BOTH | MPU_RASR_XN
                               | MPU_RASR_CACHEABLE);
    configure_region_range(MPU_UNPRIVILEGED_DATA_REGION,
                           (uintptr_t)__unprivileged_task_data_start,
                           (uintptr_t)__unprivileged_task_data_end,
                           unprivileged_data_attributes);
}
#endif

volatile uint32_t g_mpu_stack_guard_base KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_mpu_stack_guard_updates KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_mpu_private_data_base KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_mpu_private_data_size KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_mpu_private_data_updates KERNEL_PRIVILEGED_DATA;

void arch_set_task_private_data(void *private_data_base, uint32_t private_data_size)
{
    uint32_t base = (uint32_t)(uintptr_t)private_data_base;

    if ((g_mpu_private_data_base == base)
        && (g_mpu_private_data_size == private_data_size))
    {
        return;
    }
#if JRT_ARCH_HAS_MPU
    MPU_RNR = MPU_TASK_PRIVATE_DATA_REGION;
    if (private_data_size == 0U)
    {
        MPU_RBAR = 0U;
        MPU_RASR = 0U;
    }
    else
    {
        uint32_t size_encoding = 4U;
        uint32_t represented_size = 32U;

        while (represented_size < private_data_size)
        {
            represented_size <<= 1U;
            size_encoding++;
        }
        MPU_RBAR = base;
        MPU_RASR = MPU_RASR_AP_READ_WRITE_BOTH | MPU_RASR_XN
            | MPU_RASR_TEX_NORMAL | MPU_RASR_CACHEABLE | MPU_RASR_BUFFERABLE
            | (size_encoding << 1U) | MPU_RASR_ENABLE;
    }
    __asm volatile ("dsb\nisb" : : : "memory");
#endif
    g_mpu_private_data_base = base;
    g_mpu_private_data_size = private_data_size;
    g_mpu_private_data_updates++;
}

void arch_set_task_stack_guard(void *guard_address)
{
    uint32_t base = (uint32_t)(uintptr_t)guard_address;

    if (g_mpu_stack_guard_base == base)
    {
        return;
    }
#if JRT_ARCH_HAS_MPU
    MPU_RNR = MPU_STACK_GUARD_REGION;
    MPU_RBAR = base;
    MPU_RASR = MPU_RASR_XN | MPU_RASR_SIZE_32_BYTES | MPU_RASR_ENABLE;
    __asm volatile ("dsb\nisb" : : : "memory");
#endif
    g_mpu_stack_guard_base = base;
    g_mpu_stack_guard_updates++;
}

void arch_configure_mpu(void *guard_address, void *private_data_base, uint32_t private_data_size)
{
#if JRT_ARCH_HAS_MPU
    uint32_t index;

    MPU_CTRL = 0U;
    __asm volatile ("dsb" : : : "memory");
    SCB_SHCSR |= SCB_SHCSR_MEMFAULTENA;
    for (index = 0U; index < MPU_REGION_COUNT; index++)
    {
        MPU_RNR = index;
        MPU_RASR = 0U;
    }
    configure_memory_regions();
    g_mpu_stack_guard_base = 0U;
    g_mpu_stack_guard_updates = 0U;
    g_mpu_private_data_base = 0U;
    g_mpu_private_data_size = 0U;
    g_mpu_private_data_updates = 0U;
    arch_set_task_private_data(private_data_base, private_data_size);
    arch_set_task_stack_guard(guard_address);
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    __asm volatile ("dsb\nisb" : : : "memory");
#else
    g_mpu_stack_guard_base = 0U;
    g_mpu_stack_guard_updates = 0U;
    g_mpu_private_data_base = 0U;
    g_mpu_private_data_size = 0U;
    g_mpu_private_data_updates = 0U;
    arch_set_task_private_data(private_data_base, private_data_size);
    arch_set_task_stack_guard(guard_address);
#endif
}

void tick_init(void)
{
    SCB_SHPR3 = (SCB_SHPR3 & 0x0000FFFFUL)
          | (CORTEXM_PRIORITY_VALUE(PENDSV_LOGICAL_PRIORITY)
              << SCB_SHPR3_PENDSV_SHIFT)
          | (CORTEXM_PRIORITY_VALUE(SYSTICK_LOGICAL_PRIORITY)
              << SCB_SHPR3_SYSTICK_SHIFT);
    SYST_RVR = JRT_SYSTICK_RELOAD;
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CSR_CLKSOURCE;
}

void request_switch(void)
{
    SCB_ICSR = SCB_ICSR_PENDSVSET;
}

void arch_request_switch(void)
{
    request_switch();
}

void arch_tick_init(void)
{
#if JRT_ARCH_FPU_CONTEXT
    SCB_CPACR |= SCB_CPACR_CP10_CP11_FULL_ACCESS;
    __asm volatile ("dsb\n" "isb\n" : : : "memory");
    FPU_FPCCR |= FPU_FPCCR_ASPEN | FPU_FPCCR_LSPEN;
#endif
    tick_init();
}

void arch_tick_start(void)
{
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
}

int kernel_in_isr(void)
{
    uint32_t ipsr;

    __asm volatile (
        "mrs %0, ipsr\n"
        : "=r" (ipsr)
        :
        : "memory");

    return (ipsr != 0U) ? 1 : 0;
}

uint32_t critical_enter(void)
{
    uint32_t saved_primask;

    __asm volatile (
        "mrs %0, primask\n"
        "cpsid i\n"
        : "=r" (saved_primask)
        :
        : "memory");

    return saved_primask;
}

void critical_exit(uint32_t saved_primask)
{
    __asm volatile (
        "msr primask, %0\n"
        :
        : "r" (saved_primask)
        : "memory");
}

int arch_in_isr(void)
{
    return kernel_in_isr();
}

uint32_t arch_critical_enter(void)
{
    return critical_enter();
}

void arch_critical_exit(uint32_t saved_primask)
{
    critical_exit(saved_primask);
}

void arch_yield(void)
{
    JRT_TaskYield();
}

JRT_TASK_UNPRIVILEGED uint32_t JRT_MillisecondsToTicks(uint32_t milliseconds)
{
    uint32_t whole_seconds = milliseconds / 1000U;
    uint32_t remaining_milliseconds = milliseconds % 1000U;
    uint32_t whole_ticks;
    uint32_t remaining_ticks;
    uint32_t fractional_ticks;

    if (whole_seconds > (UINT32_MAX / JRT_TICK_RATE_HZ))
    {
        return UINT32_MAX;
    }
    whole_ticks = whole_seconds * JRT_TICK_RATE_HZ;
    remaining_ticks = remaining_milliseconds * (JRT_TICK_RATE_HZ / 1000U);
    fractional_ticks = remaining_milliseconds * (JRT_TICK_RATE_HZ % 1000U);
    remaining_ticks += (fractional_ticks + 999U) / 1000U;
    if (whole_ticks > (UINT32_MAX - remaining_ticks))
    {
        return UINT32_MAX;
    }
    return whole_ticks + remaining_ticks;
}

void arch_wait_for_interrupt(void)
{
    __asm volatile ("wfi" : : : "memory");
}

void arch_start_first_task(void) __attribute__((naked, noreturn));

void arch_start_first_task(void)
{
    /*
     * CCM may have used PSP and floating point before starting justRT.  The
     * bootstrap SVC ABI requires a privileged, basic frame on MSP; task PSP
     * and FPCA state are installed by SVC_Handler from the saved task frame.
     */
    __asm volatile (
        "mrs r0, control\n"
        "bic r0, r0, #7\n"
        "msr control, r0\n"
        "isb\n"
        "cpsie i\n"
        "svc 0\n"
        "b .\n"
        : : : "memory");
}

void SysTick_Handler(void)
{
#if JRT_ENABLE_TEST_HOOKS
    JRT_KernelTickHook_t hook = tick_hook;
#endif

    tick_tasks();
#if JRT_ENABLE_TEST_HOOKS
    if (hook != 0)
    {
        hook();
    }
#endif
    request_switch();
}

void svc_dispatch(uint32_t *stacked_frame, uint32_t exc_return)
{
    uint8_t svc_number = ((const uint8_t *)stacked_frame[6])[-2];

    if ((exc_return & (1UL << 3)) == 0U
        || (exc_return & (1UL << 2)) == 0U)
    {
        g_svc_invalid_context++;
        stacked_frame[0] = JRT_STATUS_INVALID_CONTEXT;
        return;
    }

    switch (svc_number)
    {
        case SVC_SERVICE_YIELD:
            break;
        case SVC_SERVICE_SLEEP:
            sleep_current(stacked_frame[0]);
            break;
        case SVC_SERVICE_LED_TOGGLE:
            board_led_toggle();
            break;
        case SVC_SERVICE_TASK_SUSPEND:
            stacked_frame[0] = kernel_task_suspend(stacked_frame[0]);
            break;
        case SVC_SERVICE_TASK_RESUME:
            stacked_frame[0] = kernel_task_resume(stacked_frame[0]);
            break;
        default:
            g_svc_invalid_service++;
            return;
    }
    request_switch();
}

void PendSV_Handler(void) __attribute__((naked));

void PendSV_Handler(void)
{
    __asm volatile (
        "mrs r0, psp\n"
        "push {r3, lr}\n"
#if JRT_ARCH_FPU_CONTEXT
        "tst lr, #0x10\n"
        "it eq\n"
        "vstmdbeq r0!, {s16-s31}\n"
#endif
        "stmdb r0!, {r4-r11}\n"
        "str lr, [r0, #-4]!\n"
        "bl pendsv_switch\n"
        "mov r4, r0\n"
        "bl task_current_control\n"
        "mov r1, r0\n"
        "mov r0, r4\n"
        "bl arch_restore_task_context\n"
#if JRT_ARCH_FPU_CONTEXT
        "tst r2, #0x10\n"
        "it eq\n"
        "orreq r1, r1, #4\n"
#endif
        "msr control, r1\n"
        "isb\n"
        "pop {r3, lr}\n"
        "mov lr, r2\n"
        "bx lr\n"
    );
}
