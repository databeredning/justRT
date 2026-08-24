#include <stdint.h>

typedef struct
{
    uint8_t *ram_start;
    const uint8_t *rom_start;
    const uint8_t *rom_end;
} Sys_CopyLayoutType;

typedef struct
{
    uint8_t *ram_start;
    uint8_t *ram_end;
} Sys_ZeroLayoutType;

extern uint32_t __INIT_TABLE[];
extern uint32_t __ZERO_TABLE[];

volatile uint32_t RESET_CATCH_CORE;
void init_data_bss(void)
{
    const Sys_CopyLayoutType *copy_layout;
    const Sys_ZeroLayoutType *zero_layout;
    const uint8_t *rom;
    uint8_t *ram;
    uint32_t len;
    uint32_t size;
    uint32_t i;
    uint32_t j;

    const uint32_t *init_table_ptr = __INIT_TABLE;
    const uint32_t *zero_table_ptr = __ZERO_TABLE;

    len = *init_table_ptr;
    init_table_ptr++;
    copy_layout = (const Sys_CopyLayoutType *)init_table_ptr;
    for (i = 0U; i < len; i++)
    {
        rom = copy_layout[i].rom_start;
        ram = copy_layout[i].ram_start;
        size = (uint32_t)(copy_layout[i].rom_end - copy_layout[i].rom_start);

        for (j = 0U; j < size; j++)
        {
            ram[j] = rom[j];
        }
    }

    len = *zero_table_ptr;
    zero_table_ptr++;
    zero_layout = (const Sys_ZeroLayoutType *)zero_table_ptr;
    for (i = 0U; i < len; i++)
    {
        ram = zero_layout[i].ram_start;
        size = (uint32_t)(zero_layout[i].ram_end - zero_layout[i].ram_start);

        for (j = 0U; j < size; j++)
        {
            ram[j] = 0U;
        }
    }
}

void SystemInit(void)
{
}

void undefined_handler(void)
{
    while (1)
    {
    }
}

void NMI_Handler(void) __attribute__((weak, alias("undefined_handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("undefined_handler")));
