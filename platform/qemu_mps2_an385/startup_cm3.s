    .syntax unified
    .cpu cortex-m3
    .thumb

    .section .startup, "ax", %progbits
    .align 2
    .global Reset_Handler
    .type Reset_Handler, %function
    .thumb_func
Reset_Handler:
    cpsid i

    ldr r0, =__data_load_start
    ldr r1, =__data_start
    ldr r2, =__data_end
.Lcopy_data:
    cmp r1, r2
    bcs .Lzero_begin
    ldr r3, [r0], #4
    str r3, [r1], #4
    b .Lcopy_data

.Lzero_begin:
    ldr r1, =__zero_start
    ldr r2, =__zero_end
    movs r3, #0
.Lzero_bss:
    cmp r1, r2
    bcs .Lcall_main
    str r3, [r1], #4
    b .Lzero_bss

.Lcall_main:
    bl SystemInit
    bl main
.Lhang:
    b .Lhang
    .size Reset_Handler, .-Reset_Handler
