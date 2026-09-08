    .syntax unified
    .thumb
    .section .privileged_exceptions, "ax", %progbits
    .align 2
    .global SVC_Handler
    .type SVC_Handler, %function
    .global arch_restore_task_context
    .type arch_restore_task_context, %function

    .equ SVC_SERVICE_START_FIRST_TASK, 0

SVC_Handler:
    tst     lr, #4
    ite     eq
    mrseq   r0, msp
    mrsne   r0, psp
    ldr     r2, [r0, #24]
    ldrb    r2, [r2, #-2]
    cmp     r2, #SVC_SERVICE_START_FIRST_TASK
    bne     .Ldispatch_svc
    tst     lr, #4
    beq     .Lstart_first_task

.Ldispatch_svc:
    push    {r3, lr}
    mov     r1, lr
    bl      svc_dispatch
    pop     {r3, lr}
    bx      lr

    .size SVC_Handler, .-SVC_Handler

.Lstart_first_task:
    push    {r3, lr}
    bl      arch_tick_start
    bl      task_current_sp
    mov     r4, r0
    bl      task_current_control
    mov     r1, r0
    mov     r0, r4
    bl      arch_restore_task_context
#if JRT_ARCH_FPU_CONTEXT
    tst     r2, #0x10
    it      eq
    orreq   r1, r1, #4
#endif
    msr     control, r1
    isb
    pop     {r3, lr}
    mov     lr, r2
    bx      lr

arch_restore_task_context:
    ldr     r2, [r0], #4
    ldmia   r0!, {r4-r11}
#if JRT_ARCH_FPU_CONTEXT
    tst     r2, #0x10
    it      eq
    vldmiaeq r0!, {s16-s31}
#endif
    msr     psp, r0
    bx      lr

    .size arch_restore_task_context, .-arch_restore_task_context
