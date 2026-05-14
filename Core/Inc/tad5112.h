#ifndef TAD5112_H
#define TAD5112_H

#include "stm32h5xx_hal.h"

HAL_StatusTypeDef tad5112_init(I2C_HandleTypeDef *hi2c);
void tad5112_mute(void);
void tad5112_unmute(void);
void tad5112_sleep(void);
void tad5112_wake(void);
/* Safe to call from ISR — only sets a flag, does not do I2C. */
void tad5112_vol_up(void);
void tad5112_vol_down(void);
/* Call from thread context (does the I2C write). */
void tad5112_vol_apply(void);
/* Set volume register directly (thread context). val clamped to [TAD_VOL_MIN, TAD_VOL_MAX]. */
void tad5112_set_vol(uint8_t val);
/* Query current limit state (safe from any context after tad5112_vol_apply). */
int tad5112_at_max(void);
int tad5112_at_min(void);

#endif /* TAD5112_H */
