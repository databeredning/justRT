#include <stdint.h>

#define SYST_CSR (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018U)
#define SCB_ICSR (*(volatile uint32_t *)0xE000ED04U)
#define SCB_SHPR3 (*(volatile uint32_t *)0xE000ED20U)

#define SYST_CSR_ENABLE (1UL << 0)
#define SYST_CSR_TICKINT (1UL << 1)
#define SYST_CSR_CLKSOURCE (1UL << 2)
#define SCB_ICSR_PENDSVSET (1UL << 28)
#define SCB_SHPR3_PENDSV_SHIFT 16U
#define SCB_SHPR3_SYSTICK_SHIFT 24U

typedef struct
{
    uint8_t *ram_start;
    const uint8_t *rom_start;
    const uint8_t *rom_end;
} Sys_CopyLayoutType;

typedef struct
{
    uint8_t *ram_start;
    uint8_t *ram_end;
} Sys_ZeroLayoutType;

extern uint32_t __INIT_TABLE[];
extern uint32_t __ZERO_TABLE[];

volatile uint32_t RESET_CATCH_CORE;
volatile uint32_t g_tick_count;
volatile uint32_t g_pendsv_count;
volatile uint32_t g_systick_armed;

extern uint32_t *kernel_pendsv_switch(uint32_t *current_sp);

void kernel_tick_init(void)
{
    SCB_SHPR3 = (SCB_SHPR3 & 0x0000FFFFUL)
        | (0xFFUL << SCB_SHPR3_PENDSV_SHIFT)
        | (0xFEUL << SCB_SHPR3_SYSTICK_SHIFT);
    SYST_RVR = 15999UL;
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
    g_systick_armed = 1U;
}

void kernel_request_context_switch(void)
{
    SCB_ICSR = SCB_ICSR_PENDSVSET;
}

void init_data_bss(void)
{
    const Sys_CopyLayoutType *copy_layout;
    const Sys_ZeroLayoutType *zero_layout;
    const uint8_t *rom;
    uint8_t *ram;
    uint32_t len;
    uint32_t size;
    uint32_t i;
    uint32_t j;

    const uint32_t *init_table_ptr = __INIT_TABLE;
    const uint32_t *zero_table_ptr = __ZERO_TABLE;

    len = *init_table_ptr;
    init_table_ptr++;
    copy_layout = (const Sys_CopyLayoutType *)init_table_ptr;
    for (i = 0U; i < len; i++)
    {
        rom = copy_layout[i].rom_start;
        ram = copy_layout[i].ram_start;
        size = (uint32_t)(copy_layout[i].rom_end - copy_layout[i].rom_start);

        for (j = 0U; j < size; j++)
        {
            ram[j] = rom[j];
        }
    }

    len = *zero_table_ptr;
    zero_table_ptr++;
    zero_layout = (const Sys_ZeroLayoutType *)zero_table_ptr;
    for (i = 0U; i < len; i++)
    {
        ram = zero_layout[i].ram_start;
        size = (uint32_t)(zero_layout[i].ram_end - zero_layout[i].ram_start);

        for (j = 0U; j < size; j++)
        {
            ram[j] = 0U;
        }
    }
}

void SystemInit(void)
{
}

void undefined_handler(void)
{
    while (1)
    {
    }
}

void NMI_Handler(void) __attribute__((weak, alias("undefined_handler")));
void HardFault_Handler(void) __attribute__((weak, alias("undefined_handler")));
void MemManage_Handler(void) __attribute__((weak, alias("undefined_handler")));
void BusFault_Handler(void) __attribute__((weak, alias("undefined_handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("undefined_handler")));
void SVC_Handler(void) __attribute__((weak, alias("undefined_handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("undefined_handler")));

void SysTick_Handler(void)
{
    g_tick_count++;
    kernel_request_context_switch();
}

void PendSV_Handler(void) __attribute__((naked));

void PendSV_Handler(void)
{
    __asm volatile (
        "mrs r0, psp\n"
        "push {r3, lr}\n"
        "stmdb r0!, {r4-r11}\n"
        "bl kernel_pendsv_switch\n"
        "ldmia r0!, {r4-r11}\n"
        "msr psp, r0\n"
        "pop {r3, lr}\n"
        "bx lr\n"
    );
}
