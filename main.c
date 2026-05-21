#include <stdint.h>

volatile uint32_t g_boot_counter = 0U;
volatile uint32_t g_main_entered = 0U;
const uint32_t g_initialized_value = 0x12345678U;
uint32_t g_uninitialized_value;

int main(void)
{
    g_main_entered = 1U;
    if ((g_initialized_value != 0x12345678U) || (g_uninitialized_value != 0U))
    {
        while (1)
        {
        }
    }

    while (1)
    {
        g_boot_counter++;
    }

    return 0;
}
