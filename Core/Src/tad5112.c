#include "tad5112.h"

#define TAD5112_ADDR     0xA0U
#define TAD5112_TIMEOUT  100U

#define TAD5112_R1       0x01U  /* SW_RESET     : software reset (write 0x01) */
#define TAD5112_R2       0x02U  /* DEV_MISC_CFG : SLEEP_ENZ + DREG_EN + VREF_EN */
#define TAD5112_R26      0x1AU  /* PASI_CFG0    : format + word length     */
#define TAD5112_R118     0x76U  /* CH_EN        : output channel pair enable */
#define TAD5112_R120     0x78U  /* PWR_CFG      : DAC power-down control   */

#define TAD5112_CH1_CFG  0x64U
#define TAD5112_CH2_CFG  0x6BU
#define TAD5112_BLK_EN   0x20U  /* differential line-out, 0.6×Vref CM */

#define TAD5112_CH1A_DRV 0x65U
#define TAD5112_CH1B_DRV 0x66U
#define TAD5112_CH2A_DRV 0x6CU
#define TAD5112_CH2B_DRV 0x6DU
#define TAD5112_DRV_EN   0x20U  /* line-out driver, audio bandwidth */

#define TAD5112_VOL_CH1  0x67U
#define TAD5112_VOL_CH2  0x6EU
#define TAD5112_VOL_0DB  201U
#define TAD5112_VOL_MUTE 0x00U

#define TAD5112_VOL_MIN   21U   /* -90 dB  — practical lower bound */
#define TAD5112_VOL_MAX  221U   /* +10 dB  — practical upper bound */
#define TAD5112_VOL_STEP   6U   /*  3 dB per button press */

static I2C_HandleTypeDef *s_hi2c;
static uint8_t           s_vol       = TAD5112_VOL_0DB;
static volatile int8_t   s_vol_delta = 0;

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

    val = 0x01U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R1,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;
    HAL_Delay(2);

    /* 0x09 = SLEEP_ENZ | DREG_EN | VREF_EN — VREF required for analog output */
    val = 0x09U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R2,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;
    HAL_Delay(3);


    val = 0x30U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R26,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    vol_write(TAD5112_VOL_MUTE);

    val = TAD5112_BLK_EN;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_CH1_CFG,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    val = TAD5112_DRV_EN;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_CH1A_DRV,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_CH1B_DRV,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    val = TAD5112_BLK_EN;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_CH2_CFG,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    val = TAD5112_DRV_EN;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_CH2A_DRV,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_CH2B_DRV,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    val = 0x0CU;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R118,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    if (st != HAL_OK) return st;

    val = 0x40U;
    st = HAL_I2C_Mem_Write(hi2c, TAD5112_ADDR, TAD5112_R120,
                            I2C_MEMADD_SIZE_8BIT, &val, 1, TAD5112_TIMEOUT);
    return st;
}

void tad5112_mute(void)   { vol_write(TAD5112_VOL_MUTE); }
void tad5112_unmute(void) { vol_write(s_vol); }

void tad5112_vol_up(void)   { s_vol_delta++; }
void tad5112_vol_down(void) { s_vol_delta--; }

void tad5112_vol_apply(void)
{
    int8_t d = s_vol_delta;
    if (d == 0) return;
    s_vol_delta = 0;
    int16_t v = (int16_t)s_vol + (int16_t)d * (int16_t)TAD5112_VOL_STEP;
    if (v < (int16_t)TAD5112_VOL_MIN) v = (int16_t)TAD5112_VOL_MIN;
    if (v > (int16_t)TAD5112_VOL_MAX) v = (int16_t)TAD5112_VOL_MAX;
    s_vol = (uint8_t)v;
    vol_write(s_vol);
}

void tad5112_set_vol(uint8_t val)
{
    if (val < TAD5112_VOL_MIN) val = TAD5112_VOL_MIN;
    if (val > TAD5112_VOL_MAX) val = TAD5112_VOL_MAX;
    s_vol = val;
    vol_write(s_vol);
}

int tad5112_at_max(void) { return s_vol >= TAD5112_VOL_MAX; }
int tad5112_at_min(void) { return s_vol <= TAD5112_VOL_MIN; }

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
