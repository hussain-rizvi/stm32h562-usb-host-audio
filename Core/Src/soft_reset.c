#include "main.h"

#define SOFT_RESET_MAGIC  0xCA11AB1EUL

/* Survives NVIC_SystemReset; cleared to random on VDD power-cycle. */
__attribute__((section(".noinit"), used))
static volatile uint32_t s_soft_reset_mark;

void system_soft_reset(void)
{
    s_soft_reset_mark = SOFT_RESET_MAGIC;
    NVIC_SystemReset();
}

/* Returns 1 if this boot was triggered by system_soft_reset(), 0 otherwise.
   Clears the mark so it does not persist to the next boot. */
int system_is_soft_reset(void)
{
    if (s_soft_reset_mark == SOFT_RESET_MAGIC)
    {
        s_soft_reset_mark = 0;
        return 1;
    }
    return 0;
}
