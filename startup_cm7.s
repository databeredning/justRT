#define MAIN_CORE 0
#define MCME_CTL_KEY    0x402DC000
#define MCME_PRTN1_PUPD 0x402DC304
#define MCME_PRTN1_COFB0_CLKEN 0x402DC330
#define MCME_PRTN1_COFB0_STAT 0x402DC310
#define MCME_MSCM_REQ (1 << 24)
#define MCME_KEY 0x5AF0
#define MCME_INV_KEY 0xA50F
#define CM7_ITCMCR 0xE000EF90
#define CM7_DTCMCR 0xE000EF94

#define SBAF_BOOT_MARKER   (0x5AA55AA5)
#define CM7_0_ENABLE_SHIFT (0)
#define CM7_1_ENABLE_SHIFT (1)

#define CM7_0_ENABLE            (1)
#define CM7_1_ENABLE            (0)
#define CM7_0_VTOR_ADDR         (__CORE0_VTOR)
#define CM7_1_VTOR_ADDR         (__CORE1_VTOR)
#define XRDC_CONFIG_ADDR        (0)
#define LF_CONFIG_ADDR          (0)

    .syntax unified
    .arch armv7-m
/*
 * Runtime init tables consumed by init_data_bss() in system.c:
 * - .init_table describes ROM->RAM copy ranges for initialized data
 * - .zero_table describes RAM ranges to clear for BSS
 */
/* Copy table:
  - Table entries count
    - entry one ram start
    - entry one rom start
    - entry one rom end
    ...
    - entry n ram start
    - entry n rom start
    - entry n rom end
  Zero Table:
    - Table entries count
      - entry one ram start
      - entry one ram end
*/
.section ".init_table", "a"
  .long 4
  .long __RAM_CACHEABLE_START
  .long __ROM_CACHEABLE_START
  .long __ROM_CACHEABLE_END
  .long __RAM_NO_CACHEABLE_START
  .long __ROM_NO_CACHEABLE_START
  .long __ROM_NO_CACHEABLE_END
  .long __RAM_SHAREABLE_START
  .long __ROM_SHAREABLE_START
  .long __ROM_SHAREABLE_END
  .long __RAM_INTERRUPT_START
  .long __ROM_INTERRUPT_START
  .long __ROM_INTERRUPT_END  
.section ".zero_table", "a"
  .long 3
  .long __BSS_SRAM_SH_START
  .long __BSS_SRAM_SH_END
  .long __BSS_SRAM_NC_START
  .long __BSS_SRAM_NC_END
  .long __BSS_SRAM_START
  .long __BSS_SRAM_END

.globl RESET_CATCH_CORE
.section ".boot_header","ax"
  /* Boot header consumed by device boot ROM / SBAF. */
  .long SBAF_BOOT_MARKER /* IVT marker */
  .long (CM7_0_ENABLE << CM7_0_ENABLE_SHIFT) | (CM7_1_ENABLE << CM7_1_ENABLE_SHIFT) /* Boot configuration word */
  .long 0 /* Reserved */
  .long CM7_0_VTOR_ADDR /* CM7_0 Start address */
  .long 0 /* Reserved */
  .long CM7_1_VTOR_ADDR /* CM7_1 Start address */
  .long 0 /* Reserved */
  .long 0 /* Reserved */
  .long XRDC_CONFIG_ADDR /* XRDC configuration pointer */
  .long LF_CONFIG_ADDR /* Lifecycle configuration pointer */
  .long 0 /* Reserved */

.section ".startup","ax"
.thumb
.set VTOR_REG, 0xE000ED08
.thumb_func
.globl Reset_Handler
Reset_Handler:
/*
 * Reset entry:
 * - keep interrupts masked during low-level init
 * - clear caller-saved registers to start from known state
 */
 cpsid i
 mov   r0, #0
 mov   r1, #0
 mov   r2, #0
 mov   r3, #0
 mov   r4, #0
 mov   r5, #0
 mov   r6, #0
 mov   r7, #0

  /* If MSCM clock is already enabled, skip the update sequence. */
  ldr r0, =MCME_PRTN1_COFB0_STAT
  ldr r1, [r0]
  ldr r2, =MCME_MSCM_REQ
  and r1, r1, r2
  cmp r1, 0
  bne SetVTOR

  /* Request MSCM clock in partition 1 clock enable register. */
  ldr r0, =MCME_PRTN1_COFB0_CLKEN
  ldr r1, [r0]
  ldr r2, =MCME_MSCM_REQ
  orr r1, r2
  str r1, [r0]

  /* Mark partition update pending. */
  ldr r0, =MCME_PRTN1_PUPD
  ldr r1, [r0]
  ldr r2, =1
  orr r1, r2 
  str r1, [r0]

  /* Commit partition update using key + inverse key sequence. */
  ldr r0, =MCME_CTL_KEY
  ldr r1, =MCME_KEY
  str r1, [r0]
  ldr r1, =MCME_INV_KEY
  str r1, [r0]
/* Wait until hardware reports MSCM clock is active. */
WaitForClock:
  ldr r0, =MCME_PRTN1_COFB0_STAT
  ldr r1, [r0]
  ldr r2, =MCME_MSCM_REQ
  and r1, r1, r2
  cmp r1, 0
  beq WaitForClock

SetVTOR:
/* Use the immutable flash vector table for exception dispatch. */
ldr  r0, =VTOR_REG
ldr  r1, =__CORE0_VTOR
str  r1,[r0]

/* Read core ID and choose the core-specific stack pointer. */
ldr  r0, =0x40260004
ldr  r1,[r0]

ldr  r0, =MAIN_CORE
cmp  r1,r0
beq	 SetCore0Stack
b SetCore1Stack

SetCore0Stack:
  /* Set Main Stack Pointer for core 0, then disable SWT0. */
  ldr  r0, =__Stack_start_c0
  msr MSP, r0
  b DisableSWT0

SetCore1Stack:
  /* Set Main Stack Pointer for core 1 and continue memory init. */
  ldr  r0, =__Stack_start_c1
  msr MSP, r0
  b DTCM_Init /* SWT1 clock is disabled at startup */

/* Disable SWT0 watchdog on core 0 path. */
DisableSWT0:
  ldr  r0, =0x40270010
  ldr  r1, =0xC520
  str  r1, [r0]
  ldr  r1, =0xD928
  str  r1, [r0]
  ldr  r0, =0x40270000
  ldr  r1, =0xFF000040
  str  r1, [r0]
  b    RamInit

/* Initialize SRAM contents to seed ECC before first normal accesses. */
RamInit:
    /* Initialize SRAM ECC */
    ldr  r0, =__RAM_INIT
    cmp  r0, 0
    /* Skip if __SRAM_INIT is not set */
    beq SRAM_LOOP_END
    ldr r1, =__INT_SRAM_START
    ldr r2, =__INT_SRAM_END
    
    subs    r2, r1
    subs    r2, #1
    ble SRAM_LOOP_END

    movs    r0, 0
    movs    r3, 0
SRAM_LOOP:
    stm r1!, {r0,r3}
    subs r2, 8
    bge SRAM_LOOP
SRAM_LOOP_END:

DTCM_Init:
  /* Initialize DTCM and seed ECC. */
    ldr  r0, =__DTCM_INIT
    cmp  r0, 0
    /* Skip if __DTCM_INIT is not set */
    beq DTCM_LOOP_END
    /* Enable TCM */
    LDR r1, =CM7_DTCMCR
    LDR r0, [r1]
    LDR r2, =0x1
    ORR r0, r2
    STR r0, [r1]

    ldr r1, =__INT_DTCM_START
    ldr r2, =__INT_DTCM_END
    
    subs    r2, r1
    subs    r2, #1
    ble DTCM_LOOP_END

    movs    r0, 0
    movs    r3, 0
DTCM_LOOP:
    stm r1!, {r0,r3}
    subs r2, #8
    bge DTCM_LOOP
DTCM_LOOP_END:

ITCM_Init:
  /* Initialize ITCM and seed ECC. */
    ldr  r0, =__ITCM_INIT
    cmp  r0, 0
    /* Skip if __TCM_INIT is not set */
    beq ITCM_LOOP_END

    /* Enable TCM */
    LDR r1, =CM7_ITCMCR
    LDR r0, [r1]
    LDR r2, =0x1
    ORR r0, r2
    STR r0, [r1]

    ldr r1, =__INT_ITCM_START
    ldr r2, =__INT_ITCM_END
    
    subs    r2, r1
    subs    r2, #1
    ble ITCM_LOOP_END

    movs    r0, 0
    movs    r3, 0
ITCM_LOOP:
    stm r1!, {r0,r3}
    subs r2, #8
    bge ITCM_LOOP
ITCM_LOOP_END:

DebuggerHeldCoreLoop:
  /* Optional debugger gate: hold here while RESET_CATCH_CORE is magic value. */
  ldr  r0, =RESET_CATCH_CORE
  ldr  r0, [r0]
  ldr  r1, =0x5A5A5A5A
  cmp  r0, r1
  beq	DebuggerHeldCoreLoop

/* Main core performs data copy and BSS clear; secondary core skips it. */
_DATA_INIT:
  /* If this is the primary core, initialize data and bss */
  ldr  r0, =0x40260004
  ldr  r1,[r0]

  ldr  r0, =MAIN_CORE
  cmp  r1,r0
  beq	 _INIT_DATA_BSS
  b    __SYSTEM_INIT

_INIT_DATA_BSS:
  /* Copy initialized data and zero BSS using linker-generated tables. */
  bl init_data_bss

__SYSTEM_INIT:
  /* Hook for clock/peripheral setup if needed by the project. */
  bl SystemInit

/*********************************/
/* Set the small ro data pointer */
/*********************************/


/*********************************/
/* Set the small rw data pointer */
/*********************************/

/******************************************************************/
/* Call Main Routine                                              */
/******************************************************************/
_MAIN:
  /* Application entry point. Interrupts remain masked unless app enables them. */
  bl main

/******************************************************************/
/* Init runtime check data space                                  */
/******************************************************************/
.globl MCAL_LTB_TRACE_OFF
 MCAL_LTB_TRACE_OFF:
  /* Reserved trace hook label kept for compatibility. */
    nop

  /* Safety fallback if main() returns. */
.globl _end_of_eunit_test
_end_of_eunit_test:
    b .
