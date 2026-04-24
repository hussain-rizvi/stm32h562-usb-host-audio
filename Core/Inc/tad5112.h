#ifndef TAD5112_H
#define TAD5112_H

#include "stm32h5xx_hal.h"

HAL_StatusTypeDef tad5112_init(I2C_HandleTypeDef *hi2c);

#endif /* TAD5112_H */
