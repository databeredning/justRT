#ifndef JUSTRT_BOARD_H
#define JUSTRT_BOARD_H

#define BOARD_PRIVILEGED __attribute__((section(".privileged_functions")))

void board_init(void) BOARD_PRIVILEGED;
void board_led_toggle(void) BOARD_PRIVILEGED;

#endif
