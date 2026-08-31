ENTRY(Reset_Handler)

MEMORY
{
    flash (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00400000
    sram  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00400000
}

SECTIONS
{
    /DISCARD/ : { *(.ARM.exidx*) *(.gnu.linkonce.armexidx.*) }

    .flash :
    {
        KEEP(*(.intc_vector))
        KEEP(*(.startup))
    } > flash

    .privileged_functions :
    {
        __privileged_functions_start = .;
        *(.privileged_functions .privileged_functions.*)
        __privileged_functions_end = .;
    } > flash

    .privileged_exceptions :
    {
        *(.privileged_exceptions .privileged_exceptions.*)
    } > flash

    .unprivileged_functions :
    {
        __unprivileged_functions_start = .;
        *(.unprivileged_functions .unprivileged_functions.*)
        __unprivileged_functions_end = .;
    } > flash

    .unprivileged_svc :
    {
        __unprivileged_svc_start = .;
        *(.unprivileged_svc .unprivileged_svc.*)
        __unprivileged_svc_end = .;
    } > flash

    .unprivileged_rodata :
    {
        __unprivileged_rodata_start = .;
        *(.unprivileged_rodata .unprivileged_rodata.*)
        __unprivileged_rodata_end = .;
    } > flash

    .text :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        __flash_end = .;
    } > flash

    __data_load_start = __flash_end;
    .data : AT(__data_load_start)
    {
        . = ALIGN(8);
        __data_start = .;
        *(.privileged_data .privileged_data.*)
        *(.data .data.*)
        . = ALIGN(8);
        __data_end = .;
    } > sram

    .unprivileged_task_data (NOLOAD) :
    {
        . = ALIGN(32);
        __zero_start = .;
        __unprivileged_task_data_start = .;
        *(.unprivileged_task_data .unprivileged_task_data.*)
        __unprivileged_task_data_end = .;
    } > sram

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __zero_end = .;
    } > sram

    __StackTop = ORIGIN(sram) + LENGTH(sram);
}

ASSERT(__data_end <= __zero_start, "QEMU data sections overlap")
ASSERT(__zero_end <= __StackTop, "QEMU SRAM overflow")
