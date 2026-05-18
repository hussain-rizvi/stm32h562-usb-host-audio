#include "audio_config.h"
#include "main.h"

/* ── LED ─────────────────────────────────────────────────────────────────── */

#define MS_TO_TICKS(ms)  ((ULONG)(ms) * TX_TIMER_TICKS_PER_SECOND / 1000U)

static TX_TIMER          s_led_timer;
static volatile LedState s_led_state = LED_OFF;
static uint8_t           s_tick;

/* PA0/PA1 ready LED */
static uint8_t s_ready_on = 0;

/* PA2/PA3 volume LEDs */
static uint8_t s_vol_playing   = 0;
static uint8_t s_vol2_pressing = 0;  /* up button currently held */
static uint8_t s_vol2_at_limit = 0;  /* at max while held */
static uint8_t s_vol3_pressing = 0;  /* down button currently held */
static uint8_t s_vol3_at_limit = 0;  /* at min while held */

static VOID led_cb(ULONG arg)
{
    (void)arg;
    s_tick++;
    uint8_t on;
    switch (s_led_state)
    {
    case LED_OFF:
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_RESET);
        break;
    case LED_ON:
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_SET);
        break;
    case LED_BLINK_NO_CARD:
        /* 500 ms on / 500 ms off at 100 ms/tick → toggle every 5 ticks */
        on = (s_tick % 10U) < 5U;
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin,
                          on ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    case LED_BLINK_NO_AUDIO:
        /* 200 ms on / 200 ms off at 100 ms/tick → toggle every 2 ticks */
        on = (s_tick % 4U) < 2U;
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin,
                          on ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    }

    /* PA0/PA1 — ready-to-play indicator (solid on/off, no blink) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1,
                      s_ready_on ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* PA2 — vol-up LED: blink while held, fast-blink if at max, solid when released */
    if (s_vol2_pressing)
    {
        on = s_vol2_at_limit ? ((s_tick % 2U) == 0U) : ((s_tick % 8U) < 4U);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2,
                          s_vol_playing ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    /* PA3 — vol-down LED: blink while held, fast-blink if at min, solid when released */
    if (s_vol3_pressing)
    {
        on = s_vol3_at_limit ? ((s_tick % 2U) == 0U) : ((s_tick % 8U) < 4U);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3,
                          s_vol_playing ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void config_led_init(void)
{
    s_tick      = 0;
    s_led_state = LED_OFF;
    tx_timer_create(&s_led_timer, "led", led_cb, 0,
                    MS_TO_TICKS(100U), MS_TO_TICKS(100U), TX_NO_ACTIVATE);
}

void led_version_blink(void)
{
    /* Major version: FW_VERSION_MAJOR long pulses (800 ms on, 300 ms off) */
    for (UINT i = 0U; i < FW_VERSION_MAJOR; i++)
    {
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_SET);
        HAL_Delay(800U);
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_RESET);
        HAL_Delay(300U);
    }
    HAL_Delay(1000U);  /* gap between major and minor groups */

    /* Minor version: FW_VERSION_MINOR short pulses (500 ms on, 200 ms off) */
    for (UINT i = 0U; i < FW_VERSION_MINOR; i++)
    {
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_SET);
        HAL_Delay(500U);
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_RESET);
        HAL_Delay(200U);
    }
    HAL_Delay(1000U);  /* end pause before LED timer takes over */
}

void led_timer_start(void)
{
    s_tick = 0;
    tx_timer_activate(&s_led_timer);
}

void led_set_state(LedState state)
{
    s_led_state = state;
}

void ready_led_set(int on)
{
    s_ready_on = on ? 1U : 0U;
}

void vol_led_set_playing(int is_playing)
{
    s_vol_playing  = is_playing ? 1U : 0U;
    if (!is_playing) { s_vol2_pressing = 0U; s_vol3_pressing = 0U; }
}

void vol_led_update(int dn_pressing, int dn_at_limit, int up_pressing, int up_at_limit)
{
    s_vol3_pressing = dn_pressing  ? 1U : 0U;
    s_vol3_at_limit = dn_at_limit  ? 1U : 0U;
    s_vol2_pressing = up_pressing  ? 1U : 0U;
    s_vol2_at_limit = up_at_limit  ? 1U : 0U;
}
