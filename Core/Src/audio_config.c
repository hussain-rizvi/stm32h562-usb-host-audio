#include "audio_config.h"
#include "main.h"
#include <string.h>

AudioConfig g_config = {
    .default_output = 1,   /* Analog */
    .default_vol    = 6,
    .max_vol        = 10,
    .min_vol        = 1,
};

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
    s_tick     = 0;
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

/* ── Config parsing ──────────────────────────────────────────────────────── */

void config_load_defaults(void)
{
    g_config.default_output = 1;
    g_config.default_vol    = 6;
    g_config.max_vol        = 10;
    g_config.min_vol        = 1;
}

static uint8_t to_lower_c(uint8_t c)
{
    return (c >= (uint8_t)'A' && c <= (uint8_t)'Z') ? c | 0x20U : c;
}

/* Case-insensitive equality — both strings must have same length. */
static int str_eq_i(const char *a, const char *b)
{
    while (*b)
    {
        if (to_lower_c((uint8_t)*a) != to_lower_c((uint8_t)*b))
            return 0;
        a++; b++;
    }
    return (*a == '\0');
}

static uint8_t parse_u8(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    uint16_t v = 0;
    uint8_t  n = 0;
    while (*s >= '0' && *s <= '9')
    {
        v = (uint16_t)(v * 10U + (uint16_t)(*s - '0'));
        if (v > 255U) return 0xFFU;
        n++; s++;
    }
    return (n > 0U) ? (uint8_t)v : 0xFFU;
}

static void trim_ws(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (uint8_t)s[n - 1] <= ' ')
        s[--n] = '\0';
}

static void apply_kv(char *key, char *val)
{
    trim_ws(key);
    while (*val == ' ' || *val == '\t') val++;
    trim_ws(val);

    if (str_eq_i(key, "defaultOutput"))
    {
        if      (str_eq_i(val, "USB"))    g_config.default_output = 0;
        else if (str_eq_i(val, "Analog")) g_config.default_output = 1;
        else if (str_eq_i(val, "SAI"))    g_config.default_output = 1;
    }
    else if (str_eq_i(key, "defaultVol"))
    {
        uint8_t v = parse_u8(val);
        if (v != 0xFFU) g_config.default_vol = v;
    }
    else if (str_eq_i(key, "MaxVol"))
    {
        uint8_t v = parse_u8(val);
        if (v != 0xFFU) g_config.max_vol = v;
    }
    else if (str_eq_i(key, "MinVol"))
    {
        uint8_t v = parse_u8(val);
        if (v != 0xFFU) g_config.min_vol = v;
    }
}

void config_read_from_sd(FX_MEDIA *media)
{
    FX_FILE f;
    if (fx_file_open(media, &f, "config.txt", FX_OPEN_FOR_READ) != FX_SUCCESS)
        return;

    char  buf[256];
    ULONG nr = 0;
    fx_file_read(&f, buf, sizeof(buf) - 1U, &nr);
    fx_file_close(&f);
    buf[nr] = '\0';

    char *p = buf;
    while (*p)
    {
        while (*p == '\r' || *p == '\n') p++;
        if (!*p) break;

        char *line = p;
        while (*p && *p != '\r' && *p != '\n') p++;
        char save = *p;
        *p = '\0';

        /* Skip comments */
        char *k = line;
        while (*k == ' ' || *k == '\t') k++;
        if (*k != '#' && *k != ';' && *k != '\0')
        {
            char *eq = strchr(k, '=');
            if (eq) { *eq = '\0'; apply_kv(k, eq + 1); }
        }
        *p = save;
    }

    /* Sanity: keep range valid and default within range */
    if (g_config.min_vol >= g_config.max_vol) { g_config.min_vol = 1; g_config.max_vol = 10; }
    if (g_config.default_vol < g_config.min_vol) g_config.default_vol = g_config.min_vol;
    if (g_config.default_vol > g_config.max_vol) g_config.default_vol = g_config.max_vol;
}


/* ── Volume mapping ──────────────────────────────────────────────────────── */

/* TAD5112: val = 201 + (gain_dB * 2), range 21 (~-90 dB) to 201 (0 dB) */
/* TAD5112: val = 201 + (gain_dB * 2).
   Map the 1-10 user scale over exactly (max_vol - min_vol) button steps of 3 dB each
   (TAD5112_VOL_STEP = 6 register units = 3 dB), anchored at 0 dB for vol=max_vol.
   vol=10 → 201 (0 dB), vol=6 → 177 (-12 dB), vol=1 → 147 (-27 dB). */
#define TAD_VOL_0DB    201U
#define TAD_VOL_STEP     6U   /* 3 dB per step, matches tad5112_vol_up/down */

uint8_t config_vol_to_tad(void)
{
    uint8_t range = g_config.max_vol - g_config.min_vol;
    if (range == 0U) return (uint8_t)TAD_VOL_0DB;
    /* Steps below max = (max_vol - default_vol) button presses down from 0 dB */
    uint8_t steps_down = g_config.max_vol - g_config.default_vol;
    uint8_t val = (uint8_t)(TAD_VOL_0DB - (UINT)steps_down * TAD_VOL_STEP);
    return val;
}

/* USB vol uses the device's own resolution (step) so each user-scale step equals
   one button press.  practical_min is anchored (max_vol - min_vol) steps below
   vol_max; hardware vol_min is used only as a lower bound. */
int32_t config_vol_to_usb(int32_t vol_min, int32_t vol_max, int32_t step)
{
    uint8_t range = g_config.max_vol - g_config.min_vol;
    if (range == 0U || vol_min >= vol_max) return vol_max;
    int32_t practical_min = vol_max - (int32_t)range * step;
    if (practical_min < vol_min) practical_min = vol_min;
    return practical_min + (int32_t)(g_config.default_vol - g_config.min_vol)
                           * (vol_max - practical_min) / (int32_t)range;
}
