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
