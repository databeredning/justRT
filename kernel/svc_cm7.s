    .syntax unified
    .thumb
    .section .system_calls, "ax", %progbits
    .align 2
    .global SVC_Handler
    .type SVC_Handler, %function

SVC_Handler:
    tst     lr, #4
    ite     eq
    mrseq   r0, msp
    mrsne   r0, psp
    push    {r3, lr}
    mov     r1, lr
    bl      svc_dispatch
    pop     {r3, lr}
    bx      lr

    .size SVC_Handler, .-SVC_Handler
