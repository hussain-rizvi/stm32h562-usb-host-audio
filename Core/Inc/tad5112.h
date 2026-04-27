#ifndef TAD5112_H
#define TAD5112_H

#include "stm32h5xx_hal.h"

HAL_StatusTypeDef tad5112_init(I2C_HandleTypeDef *hi2c);
void tad5112_mute(void);
void tad5112_unmute(void);

#endif /* TAD5112_H */
