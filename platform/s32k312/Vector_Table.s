/* justRT interrupt vector table for the S32K312 Cortex-M7 target. */

.section  ".intc_vector","ax"
.align 2
.thumb
.globl undefined_handler
.globl VTABLE
.globl __Stack_start_c0          /* Top of Stack for Initial Stack Pointer */
.globl Reset_Handler             /* Reset Handler */
.globl NMI_Handler               /* NMI Handler */
.globl HardFault_Handler         /* Hard Fault Handler */
.globl MemManage_Handler         /* Reserved */
.globl BusFault_Handler          /* Bus Fault Handler */
.globl UsageFault_Handler        /* Usage Fault Handler */
.globl SVC_Handler               /* SVCall Handler */
.globl DebugMon_Handler          /* Debug Monitor Handler */
.globl PendSV_Handler            /* PendSV Handler */
.globl SysTick_Handler           /* SysTick Handler */ /* 15*/

VTABLE:

.long __Stack_start_c0          /* Top of Stack for Initial Stack Pointer */
.long Reset_Handler+1           /* Reset Handler need plus 1 because Reset_Handler is generated with LSB bit =0*/
.long NMI_Handler+1             /* NMI Handler */
.long HardFault_Handler+1       /* Hard Fault Handler */
.long MemManage_Handler+1       /* MemManage Handler */
.long BusFault_Handler+1        /* Bus Fault Handler */
.long UsageFault_Handler+1      /* Usage Fault Handler */
.long 0                         /* Reserved */
.long 0                         /* Reserved */
.long 0                         /* Reserved */
.long 0                         /* Reserved */
.long SVC_Handler+1             /* SVCall Handler */
.long DebugMon_Handler+1        /* Debug Monitor Handler */
.long 0                         /* Reserved */
.long PendSV_Handler+1          /* PendSV Handler */
.long SysTick_Handler+1         /* SysTick Handler */ /* 15*/

.long undefined_handler /*0*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*10*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler 
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*20*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler 
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*30*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler 
.long undefined_handler /*40*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler 
.long undefined_handler
.long undefined_handler /*50*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*60*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler 
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*70*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*80*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*90*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler 
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*100*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler
.long undefined_handler /*110*/
.long undefined_handler
.long undefined_handler
.long undefined_handler
