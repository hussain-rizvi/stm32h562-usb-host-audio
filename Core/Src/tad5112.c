#include "tad5112.h"

#define TAD5112_ADDR     0xA0U
#define TAD5112_TIMEOUT  100U

#define TAD5112_R2       0x02U  /* DEV_MISC_CFG : SLEEP_ENZ               */
#define TAD5112_R26      0x1AU  /* PASI_CFG0    : format + word length     */
#define TAD5112_R118     0x76U  /* CH_EN        : channel enable            */
#define TAD5112_R120     0x78U  /* PWR_CFG      : DAC power-down control   */

/* DAC_CH1A_DVOL / DAC_CH2A_DVOL: value = 201 + (gain_dB × 2), 0 = mute */
#define TAD5112_VOL_CH1  0x67U
#define TAD5112_VOL_CH2  0x6EU
#define TAD5112_VOL_0DB  201U
#define TAD5112_VOL_MUTE 0x00U

static I2C_HandleTypeDef *s_hi2c;

static void vol_write(uint8_t val)
{
    HAL_I2C_Mem_Write(s_hi2c, TAD5112_ADDR, TAD5112_VOL_CH1,
                      I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    HAL_I2C_Mem_Write(s_hi2c, TAD5112_ADDR, TAD5112_VOL_CH2,
                      I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
}

HAL_StatusTypeDef tad5112_init(I2C_HandleTypeDef *hi2c)
{
    s_hi2c = hi2c;
    HAL_StatusTypeDef st;
    uint8_t val;

    /* Wake from sleep */
    val = 0x01U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R2,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;
    HAL_Delay(3);

    /* PASI_FORMAT[7:6]=01 (I2S), PASI_WLEN[5:4]=11 (32-bit) */
    val = 0x70U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R26,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    /* Digital mute before enabling outputs */
    vol_write(TAD5112_VOL_MUTE);

    /* Enable output channels 1 and 2 */
    val = 0x0CU;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R118,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    /* Power up DAC channels */
    val = 0x40U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R120,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    return st;
}

void tad5112_mute(void)   { vol_write(TAD5112_VOL_MUTE); }
void tad5112_unmute(void) { vol_write(TAD5112_VOL_0DB);  }

void tad5112_sleep(void)
{
    uint8_t val = 0x00U;
    HAL_I2C_Mem_Write(s_hi2c, TAD5112_ADDR, TAD5112_R2,
                      I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
}

void tad5112_wake(void)
{
    uint8_t val = 0x01U;
    HAL_I2C_Mem_Write(s_hi2c, TAD5112_ADDR, TAD5112_R2,
                      I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    HAL_Delay(3);
}
