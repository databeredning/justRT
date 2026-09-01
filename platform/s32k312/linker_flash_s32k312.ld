/*
 * justRT linker script for the S32K312 Cortex-M7 core 0 target.
 *
 * This image is a freestanding, single-core firmware. It contains the
 * startup code, vector table, native RTOS kernel, and native PTB18 GPIO
 * board layer. No heap, data-flash, or flash-driver sections are required.
 */

ENTRY(Reset_Handler)

/* S32K312 memory used by this image. */
MEMORY
{
    int_flash       (rx)  : ORIGIN = 0x00400000, LENGTH = 0x00164000
    int_sram        (rwx) : ORIGIN = 0x20400100, LENGTH = 0x00013E00
    int_sram_stack  (rwx) : ORIGIN = 0x20414000, LENGTH = 0x00001000
    int_sram_nc     (rwx) : ORIGIN = 0x20415000, LENGTH = 0x00007F00
}

SECTIONS
{
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidx.*)
    }

    /* Keep the immutable vector table at the image flash origin. */
    .flash :
    {
        . = ALIGN(4);
        KEEP(*(.boot_header))
        . = ALIGN(4096);
        __ROM_INTERRUPT_START = .;
        KEEP(*(.intc_vector))
        . = ALIGN(4);
        __ROM_INTERRUPT_END = .;
        KEEP(*(.startup))
    } > int_flash

    /* Kernel and board code are the privileged executable range. */
    .privileged_functions :
    {
        . = ALIGN(4);
        __privileged_functions_start = .;
        *(.privileged_functions .privileged_functions.*)
        . = ALIGN(4);
        __privileged_functions_end = .;
    } > int_flash

    .privileged_exceptions :
    {
        . = ALIGN(4);
        *(.privileged_exceptions .privileged_exceptions.*)
        . = ALIGN(4);
    } > int_flash

    /* Example task code is the future unprivileged executable range. */
    .unprivileged_functions :
    {
        . = ALIGN(4096);
        __unprivileged_functions_start = .;
        *(.unprivileged_functions .unprivileged_functions.*)
        . = ALIGN(4096);
        __unprivileged_functions_end = .;
    } > int_flash

    .unprivileged_svc :
    {
        . = ALIGN(4096);
        __unprivileged_svc_start = .;
        *(.unprivileged_svc .unprivileged_svc.*)
        . = ALIGN(4096);
        __unprivileged_svc_end = .;
    } > int_flash

    .unprivileged_rodata :
    {
        . = ALIGN(4096);
        __unprivileged_rodata_start = .;
        *(.unprivileged_rodata .unprivileged_rodata.*)
        . = ALIGN(4096);
        __unprivileged_rodata_end = .;
    } > int_flash

    .system_calls :
    {
        . = ALIGN(4);
        __system_calls_flash_start = .;
        *(.system_calls .system_calls.*)
        . = ALIGN(4);
        __system_calls_flash_end = .;
    } > int_flash

     /* Application code, constants, and startup copy/zero tables follow the
         explicitly classified executable ranges. */
     .flash_code :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)

        . = ALIGN(4);
        __init_table = .;
        KEEP(*(.init_table))
        . = ALIGN(4);
        __zero_table = .;
        KEEP(*(.zero_table))
        . = ALIGN(4);
        __flash_end = .;
    } > int_flash

    /* Named data ranges reserved for future MPU permissions. */
    .privileged_data : AT(__flash_end)
    {
        . = ALIGN(8);
        __privileged_data_start = .;
        *(.privileged_data .privileged_data.*)
        . = ALIGN(8);
        __privileged_data_end = .;
    } > int_sram

    .unprivileged_task_data (NOLOAD) :
    {
        . = ALIGN(8192);
        __unprivileged_task_data_start = .;
        *(.unprivileged_task_data .unprivileged_task_data.*)
        . = ALIGN(8192);
        __unprivileged_task_data_end = .;
    } > int_sram

    .task_private_data (NOLOAD) :
    {
        . = ALIGN(32);
        __task_private_data_start = .;
        *(.task_private_data .task_private_data.*)
        . = ALIGN(32);
        __task_private_data_end = .;
    } > int_sram

    /* Initialized writable data is loaded immediately after flash content. */
    .sram_data : AT(LOADADDR(.privileged_data) + SIZEOF(.privileged_data))
    {
        . = ALIGN(4);
        __sram_data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __sram_data_end = .;
    } > int_sram

    /* Zero-initialized globals and static task/kernel storage. */
    .sram_bss (NOLOAD) :
    {
        . = ALIGN(8);
        __sram_bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __sram_bss_end = .;
    } > int_sram

    /* The reset stack is separate from application/task storage. */
    __Stack_end_c0 = ORIGIN(int_sram_stack);
    __Stack_start_c0 = ORIGIN(int_sram_stack) + LENGTH(int_sram_stack);
    __Stack_end_c1 = 0;
    __Stack_start_c1 = 0;

    /* Startup memory initialization symbols. */
    __INT_SRAM_START = ORIGIN(int_sram);
    __INT_SRAM_END = ORIGIN(int_sram) + LENGTH(int_sram);
    __INT_DTCM_START = 0x20000000;
    __INT_DTCM_END = 0x20010000;
    __INT_ITCM_START = 0x00000000;
    __INT_ITCM_END = 0x00008000;
    __RAM_INIT = 1;
    __DTCM_INIT = 1;
    __ITCM_INIT = 1;

    /* Runtime copy/zero table symbols consumed by system.c. */
    __INIT_TABLE = __init_table;
    __ZERO_TABLE = __zero_table;

    /* RAM vector-table relocation symbols consumed by startup_cm7.s. */
    __RAM_INTERRUPT_START = ORIGIN(int_sram_nc);
    __RAM_INTERRUPT_END = __RAM_INTERRUPT_START + (__ROM_INTERRUPT_END - __ROM_INTERRUPT_START);
    __CORE0_VTOR = __ROM_INTERRUPT_START;
    __CORE1_VTOR = __ROM_INTERRUPT_START;

    /* The current startup table has four copy entries and three zero entries.
       Empty regions are represented by equal start/end addresses. */
    __RAM_CACHEABLE_START = ORIGIN(int_sram);
    __RAM_CACHEABLE_END = __RAM_CACHEABLE_START;
    __ROM_CACHEABLE_START = __flash_end;
    __ROM_CACHEABLE_END = __flash_end;
    __RAM_NO_CACHEABLE_START = ORIGIN(int_sram_nc);
    __RAM_NO_CACHEABLE_END = __RAM_NO_CACHEABLE_START;
    __ROM_NO_CACHEABLE_START = __flash_end;
    __ROM_NO_CACHEABLE_END = __flash_end;
    __RAM_SHAREABLE_START = ORIGIN(int_sram_nc);
    __RAM_SHAREABLE_END = __RAM_SHAREABLE_START;
    __ROM_SHAREABLE_START = __flash_end;
    __ROM_SHAREABLE_END = __flash_end;
    __BSS_SRAM_SH_START = __sram_bss_end;
    __BSS_SRAM_SH_END = __sram_bss_end;
    __BSS_SRAM_NC_START = __sram_bss_end;
    __BSS_SRAM_NC_END = __sram_bss_end;
    __BSS_SRAM_START = __sram_bss_start;
    __BSS_SRAM_END = __sram_bss_end;
}

ASSERT(__RAM_INTERRUPT_END <= ORIGIN(int_sram_nc) + LENGTH(int_sram_nc),
       "RAM vector table exceeds non-cacheable SRAM")
ASSERT(__sram_bss_end <= ORIGIN(int_sram) + LENGTH(int_sram),
       "SRAM data and BSS exceed application SRAM")
