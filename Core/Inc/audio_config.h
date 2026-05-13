#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include "tx_api.h"
#include "fx_api.h"
#include <stdint.h>

/* Firmware version — shown at boot via LED blink pattern */
#define FW_VERSION_MAJOR  1U
#define FW_VERSION_MINOR  2U

typedef struct {
    uint8_t default_output;  /* 0 = USB, 1 = Analog/SAI */
    uint8_t default_vol;     /* on scale min_vol..max_vol */
    uint8_t max_vol;
    uint8_t min_vol;
} AudioConfig;

extern AudioConfig g_config;

typedef enum {
    LED_OFF            = 0,
    LED_ON             = 1,
    LED_BLINK_NO_CARD  = 2,  /* 500 ms toggle — no SD card */
    LED_BLINK_NO_AUDIO = 3,  /* 200 ms toggle — card present but no audio files */
} LedState;

/* Call once from tx_application_define (before scheduler). */
void config_led_init(void);

/* Blocking version blink — call from thread context on cold boot only. */
void led_version_blink(void);

/* Start the LED status timer — always call after led_version_blink (or instead of it). */
void led_timer_start(void);

/* Update LED blink mode. Safe to call from any thread. */
void led_set_state(LedState state);

/* Populate g_config with built-in defaults. */
void config_load_defaults(void);

/* Read config.txt from the mounted SD root; silently keeps defaults if absent. */
void config_read_from_sd(FX_MEDIA *media);

/* Write g_config.default_output to TAMP BKP0R only if no valid value is present. */
void config_apply_default_output(void);

/* Map config vol to TAD5112 DAC register value (21..201). */
uint8_t config_vol_to_tad(void);

/* Map config vol to USB Q8.8 dBFS within [vol_min..vol_max] using device step. */
int32_t config_vol_to_usb(int32_t vol_min, int32_t vol_max, int32_t step);

#endif /* AUDIO_CONFIG_H */
