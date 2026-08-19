# Run LED GPIO Setup

This document describes the verified native GPIO path for the run LED on the
S32K312. The implementation is in `board/board.c`; it does not depend on RTD
drivers or dynamic allocation.

## Hardware Mapping

| Item | Value |
| --- | --- |
| Board signal | PTB18 |
| SIUL2 pad index | 50 |
| GPIO function | SIUL2 MSCR SSS = 0 |
| MSCR register | `SIUL2 + 0x0240 + 4 * 50 = 0x40290308` |
| Parallel output register | PGPDO3 at `SIUL2 + 0x1704 = 0x40291704` |
| PTB18 bit in PGPDO3 | bit 13, mask `0x2000` |

`PGPDO3` represents the PTB16--PTB31 group. SIUL2 stores PTB18 at the
reversed physical position `15 - (50 - 48) = 13`.

The offsets above are important: MSCR does not start at the SIUL2 base, and
PGPDO3 is not part of the per-pad GPDO byte block. Earlier accesses to those
other blocks produce a precise bus fault on this target.

## Reset and System Startup

The boot header in `startup_cm7.s` tells the device boot ROM/SBAF to start
CM7_0. `Reset_Handler` then executes with interrupts masked.

Before it selects the vector table, reset code makes the MSCM peripheral
accessible. This is a separate MC_ME clock request from the LED setup:

| Register | Address | Bit/action | Purpose |
| --- | --- | --- | --- |
| `PRTN1_COFB0_STAT` | `0x402DC310` | Test bit 24 | Check whether MSCM is already clocked. |
| `PRTN1_COFB0_CLKEN` | `0x402DC330` | Set bit 24 | Request the MSCM clock. |
| `PRTN1_PUPD` | `0x402DC304` | Set `PCUD`, bit 0 | Mark the Partition 1 update pending. |
| `CTL_KEY` | `0x402DC000` | Write `0x5AF0`, then `0xA50F` | Authorize the MC_ME update. |
| `PRTN1_COFB0_STAT` | `0x402DC310` | Poll bit 24 | Wait until hardware confirms the clock is active. |

After that, startup sets `SCB->VTOR` to the immutable flash interrupt-vector table,
chooses CM7_0's MSP, disables SWT0, initializes SRAM to seed ECC, copies
initialized RAM sections, clears BSS, and enters `main()`.

`SystemInit()` currently has no clock programming. Therefore GPIO operation
does not require PLL configuration, but the observed blink period does depend
on the actual core clock because SysTick uses that clock.

## SIUL2 Clock Dependency

SIUL2 must be clocked before *any* SIUL2 register access. The heartbeat example calls
`board_init()` before task creation and before SysTick is enabled. The first
operation in `board_init()` is `enable_siul2_clock()`.

SIUL2 is MC_ME block 73, controlled through Partition 1 Clock Output Function
Block 2 (COFB2):

| Register | Address | Bit/action | Meaning |
| --- | --- | --- | --- |
| `PRTN1_COFB2_STAT` | `0x402DC318` | Test bit 9 | Check whether SIUL2/REQ73 is already active. |
| `PRTN1_PCONF` | `0x402DC300` | Set `PCE`, bit 0 | Enable Partition 1 so it accepts the update. |
| `PRTN1_COFB2_CLKEN` | `0x402DC338` | Set `REQ73`, bit 9 | Request the SIUL2 peripheral clock. |
| `PRTN1_PUPD` | `0x402DC304` | Set `PCUD`, bit 0 | Request application of the Partition 1 changes. |
| `CTL_KEY` | `0x402DC000` | Write `0x5AF0`, then `0xA50F` | Commit the requested update. |
| `PRTN1_PUPD` | `0x402DC304` | Poll until bit 0 clears | Confirm update completion. |
| `PRTN1_COFB2_STAT` | `0x402DC318` | Poll until bit 9 sets | Confirm that SIUL2 is clocked. |

The status check at the beginning makes the sequence idempotent: it does not
submit a new update if SIUL2 is already running.

## Pad Configuration

After the clock-status poll succeeds, `board_init()` configures `MSCR[50]`.
It writes `0x00280000`:

| MSCR field | Bit | Value | Effect |
| --- | --- | --- | --- |
| `SSS` | 3:0 | 0 | Select GPIO mode. |
| `IBE` | 19 | 1 | Enable the input buffer. |
| `OBE` | 21 | 1 | Enable the output buffer. |

The output latch's reset state is low, so initialization does not need to
write PGPDO3 before enabling the output buffer.

## Runtime Blink Path

The heartbeat example's LED task
calls `board_led_toggle()` directly, then sleeps for `ms_to_ticks(100)`
(`ms_to_ticks(100)`, approximately 100 ms at a 120 MHz core clock).

The privileged `board_led_toggle()` routine performs a 16-bit read-modify-write:

```c
SIUL2_PGPDO3 ^= 0x2000U;
```

This flips PTB18 while preserving the other PTB16--PTB31 output latches. The
The task sleeps for `ms_to_ticks(100)` SysTick interrupts;
`SysTick_Handler` decrements the sleep counter and requests a PendSV context
switch. With the current tick rate of 7500 Hz, the interval is approximately
100 ms.

## Ordering Requirements

1. Reset startup must make the basic system fabric available and initialize
   SRAM before normal C code uses global state.
2. `board_init()` must complete the SIUL2 clock-gate update before touching
   `MSCR[50]` or `PGPDO3`.
3. Program the output latch before enabling an output buffer when a defined
   startup level is required. This design relies on the low reset latch.
4. Enable SysTick only after `board_init()` and task stacks are ready; the
   first task may call `board_led_toggle()` immediately after task launch.

## Reference Source

The register mapping and the PGPDO3 mask were validated against the provided
`freertos-s32k312-mpu-demo/src/bsp_gpio.c` reference implementation and the
S32K312 SIUL2 register layout.