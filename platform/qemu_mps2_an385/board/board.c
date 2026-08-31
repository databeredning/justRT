#include <stdint.h>

#include "board.h"

/* MPS2 AN385 CMSDK UART0. Output is visible with QEMU -serial stdio. */
#define UART0_DATA (*(volatile uint32_t *)0x40004000U)
#define UART0_STATE (*(volatile uint32_t *)0x40004004U)
#define UART0_CTRL (*(volatile uint32_t *)0x40004008U)
#define UART_STATE_TX_FULL (1UL << 0)
#define UART_CTRL_TX_ENABLE (1UL << 0)

static uint32_t led_state;

void board_init(void)
{
    UART0_CTRL = UART_CTRL_TX_ENABLE;
}

void board_led_toggle(void)
{
    while ((UART0_STATE & UART_STATE_TX_FULL) != 0U)
    {
    }
    led_state ^= 1U;
    UART0_DATA = (led_state != 0U) ? (uint32_t)'X' : (uint32_t)'.';
}
