    .syntax unified
    .cpu cortex-m3
    .thumb

    .section .intc_vector, "a", %progbits
    .align 2
    .global VTABLE
VTABLE:
    .word __StackTop
    .word Reset_Handler + 1
    .word NMI_Handler + 1
    .word HardFault_Handler + 1
    .word MemManage_Handler + 1
    .word BusFault_Handler + 1
    .word UsageFault_Handler + 1
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler + 1
    .word DebugMon_Handler + 1
    .word 0
    .word PendSV_Handler + 1
    .word SysTick_Handler + 1

    .rept 32
    .word undefined_handler + 1
    .endr
