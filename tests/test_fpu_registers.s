    .syntax unified
    .thumb
    .fpu fpv5-sp-d16
    .section .unprivileged_functions, "ax", %progbits
    .align 2

    .global test_fpu_load_registers
    .type test_fpu_load_registers, %function
test_fpu_load_registers:
    vmov    s16, r0
    adds    r0, #1
    vmov    s17, r0
    adds    r0, #1
    vmov    s18, r0
    adds    r0, #1
    vmov    s19, r0
    adds    r0, #1
    vmov    s20, r0
    adds    r0, #1
    vmov    s21, r0
    adds    r0, #1
    vmov    s22, r0
    adds    r0, #1
    vmov    s23, r0
    adds    r0, #1
    vmov    s24, r0
    adds    r0, #1
    vmov    s25, r0
    adds    r0, #1
    vmov    s26, r0
    adds    r0, #1
    vmov    s27, r0
    adds    r0, #1
    vmov    s28, r0
    adds    r0, #1
    vmov    s29, r0
    adds    r0, #1
    vmov    s30, r0
    adds    r0, #1
    vmov    s31, r0
    bx      lr
    .size test_fpu_load_registers, .-test_fpu_load_registers

    .global test_fpu_check_registers
    .type test_fpu_check_registers, %function
test_fpu_check_registers:
    mov     r2, r0
    vmov    r1, s16
    cmp     r1, r2
    bne     .Lfpu_mismatch
.irp reg,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    adds    r2, #1
    vmov    r1, s\reg
    cmp     r1, r2
    bne     .Lfpu_mismatch
.endr
    movs    r0, #1
    bx      lr
.Lfpu_mismatch:
    movs    r0, #0
    bx      lr
    .size test_fpu_check_registers, .-test_fpu_check_registers
