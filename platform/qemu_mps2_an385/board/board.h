#ifndef JUSTRT_QEMU_MPS2_AN385_BOARD_H
#define JUSTRT_QEMU_MPS2_AN385_BOARD_H

#define BOARD_PRIVILEGED __attribute__((section(".privileged_functions")))

void board_init(void) BOARD_PRIVILEGED;
void board_led_toggle(void) BOARD_PRIVILEGED;

#endif
