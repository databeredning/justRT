#ifndef JUSTBOOT_BOARD_H
#define JUSTBOOT_BOARD_H

#include <stdint.h>

extern volatile uint32_t g_board_init_stage;

void board_init(void);
void board_led_toggle(void);

#endif
