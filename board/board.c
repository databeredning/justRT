#include <stdint.h>

#include "board.h"

/* SIUL2 registers */
#define SIUL2_BASE 0x40290000U
#define SIUL2_MSCR_BASE (SIUL2_BASE + 0x0240U)
#define SIUL2_PGPDO3 (*(volatile uint16_t *)(SIUL2_BASE + 0x1704U))

#define SIUL2_MSCR(index) (*(volatile uint32_t *)(SIUL2_MSCR_BASE + ((index) * 4U)))

#define SIUL2_MSCR_IBE (1UL << 19)
#define SIUL2_MSCR_OBE (1UL << 21)
#define PTB18_SIUL2_PIN 50U
#define PTB18_PGPDO3_MASK 0x2000U

#define SIUL2_MSCR_GPIO_BIDIR (SIUL2_MSCR_IBE | SIUL2_MSCR_OBE)

/* MC_ME clock gating */
#define MC_ME_BASE 0x402DC000U
#define MC_ME_PRTN1_PCONF (*(volatile uint32_t *)(MC_ME_BASE + 0x300U))
#define MC_ME_PRTN1_COFB2_CLKEN (*(volatile uint32_t *)(MC_ME_BASE + 0x338U))
#define MC_ME_PRTN1_COFB2_STAT (*(volatile uint32_t *)(MC_ME_BASE + 0x318U))
#define MC_ME_PRTN1_PUPD (*(volatile uint32_t *)(MC_ME_BASE + 0x304U))
#define MC_ME_CTL_KEY (*(volatile uint32_t *)(MC_ME_BASE + 0x000U))

#define MC_ME_PRTN1_PCONF_PCE (1UL << 0)
#define MC_ME_PRTN1_COFB2_REQ73 (1UL << 9)
#define MC_ME_PRTN1_PUPD_PCUD (1UL << 0)

static void enable_siul2_clock(void)
{
    /* Skip if SIUL2 is already gated on. */
    if ((MC_ME_PRTN1_COFB2_STAT & MC_ME_PRTN1_COFB2_REQ73) != 0U)
    {
        return;
    }
    
    /* Enable Partition 1 (required before partition updates). */
    MC_ME_PRTN1_PCONF |= MC_ME_PRTN1_PCONF_PCE;

    /* Request SIUL2 clock (REQ73 in COFB2). */
    MC_ME_PRTN1_COFB2_CLKEN |= MC_ME_PRTN1_COFB2_REQ73;

    /* Request partition update. */
    MC_ME_PRTN1_PUPD |= MC_ME_PRTN1_PUPD_PCUD;

    /* Authorize the update with key sequence. */
    MC_ME_CTL_KEY = 0x5AF0U;
    MC_ME_CTL_KEY = 0xA50FU;

    /* Poll for update completion and SIUL2 clock active. */
    while ((MC_ME_PRTN1_PUPD & MC_ME_PRTN1_PUPD_PCUD) != 0U || 
           (MC_ME_PRTN1_COFB2_STAT & MC_ME_PRTN1_COFB2_REQ73) == 0U)
    {
        /* Busy-wait. */
    }
}

void board_init(void)
{
    enable_siul2_clock();
    SIUL2_MSCR(PTB18_SIUL2_PIN) = SIUL2_MSCR_GPIO_BIDIR;
}

void board_led_toggle(void)
{
    SIUL2_PGPDO3 ^= PTB18_PGPDO3_MASK;
}
