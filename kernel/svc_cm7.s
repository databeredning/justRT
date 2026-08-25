    .syntax unified
    .thumb
    .section .privileged_exceptions, "ax", %progbits
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

    .section .unprivileged_svc, "ax", %progbits
    .align 2
    .global arch_enter_task
    .type arch_enter_task, %function

arch_enter_task:
    msr     control, r1
    isb
    movs    r0, #0
    movs    r1, #0
    movs    r3, #0
    bx      r2

    .size arch_enter_task, .-arch_enter_task
