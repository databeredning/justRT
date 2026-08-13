#include <stdint.h>

#include "kernel/kernel.h"

volatile uint32_t g_main_entered = 0U;
const uint32_t g_initialized_value = 0x12345678U;
uint32_t g_uninitialized_value;

static int platform_sanity_check(void)
{
    return ((g_initialized_value == 0x12345678U) && (g_uninitialized_value == 0U)) ? 0 : -1;
}

int main(void)
{
    g_boot_stage = 1U;
    g_main_entered = 1U;
    if (platform_sanity_check() != 0)
    {
        g_boot_stage = 0xEEU;
        while (1)
        {
        }
    }

    g_boot_stage = 2U;
    start();

    return 0;
}
