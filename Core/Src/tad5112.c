#include "tad5112.h"

/* 7-bit I2C address 0x50 (ADDR pin tied GND) shifted left for HAL */
#define TAD5112_ADDR     0xA0U
#define TAD5112_TIMEOUT  100U   /* ms */

/* Page-0 register addresses */
#define TAD5112_R2       0x02U  /* DEV_CTRL1 : SLEEP_ENZ — wake from sleep    */
#define TAD5112_R26      0x1AU  /* PASI_CFG  : interface format + word length  */
#define TAD5112_R118     0x76U  /* CH_EN     : output channel enable           */
#define TAD5112_R120     0x78U  /* PWR_CFG   : DAC power-down control          */

HAL_StatusTypeDef tad5112_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef st;
    uint8_t val;

    /* 1. Wake device from sleep (SLEEP_ENZ = 1) */
    val = 0x01U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R2,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    HAL_Delay(3);   /* datasheet: ≥2 ms wake-up time */

    /* 2. Set I2S format + 32-bit word length
       PASI_FORMAT[7:6]=01 (I2S), PASI_WLEN[3:2]=11 (32-bit) → 0x4C
       STM32 SAI is configured as SAI_PROTOCOL_DATASIZE_32BIT (BCLK/FSYNC=64). */
    val = 0x4CU;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R26,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    /* 3. Enable DAC output channels 1 and 2 (OUT_CH1_EN | OUT_CH2_EN = bits [3:2]) */
    val = 0x0CU;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R118,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    /* 4. Power up enabled DAC channels (DAC_PDZ = bit 6) */
    val = 0x40U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R120,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    return st;
}
