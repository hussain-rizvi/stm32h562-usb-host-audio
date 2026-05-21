#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include "tx_api.h"
#include "fx_api.h"
#include <stdint.h>

/* Firmware version — shown at boot via LED blink pattern */
#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  3

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
    LED_BLINK_ENUM_FAIL = 4, /* 100 ms toggle — USB speaker failed enumeration */
} LedState;

/* Call once from tx_application_define (before scheduler). */
void config_led_init(void);

/* Blocking version blink using HAL_Delay — call from main() before tx_kernel_enter on cold boot. */
void led_version_blink(void);

/* Start the LED status timer — always call after led_version_blink (or instead of it). */
void led_timer_start(void);

/* Update LED blink mode. Safe to call from any thread. */
void led_set_state(LedState state);

/* Populate g_config with built-in defaults. */
void config_load_defaults(void);

/* Read config.txt from the mounted SD root; silently keeps defaults if absent. */
void config_read_from_sd(FX_MEDIA *media);

/* Ready-to-play indicator: PA0 and PA1 solid ON when system is ready. */
void ready_led_set(int on);

/* Volume button LEDs: PA2 = vol-up, PA3 = vol-down.
   Solid ON during playback; blink while button is held; fast-blink when held at the limit.
   Call vol_led_update() every poll tick with current button state and limit flags. */
void vol_led_set_playing(int is_playing);
void vol_led_update(int dn_pressing, int dn_at_limit, int up_pressing, int up_at_limit);

/* Map config vol to TAD5112 DAC register value (21..201). */
uint8_t config_vol_to_tad(void);

/* Map config vol to USB Q8.8 dBFS within [vol_min..vol_max] using device step. */
int32_t config_vol_to_usb(int32_t vol_min, int32_t vol_max, int32_t step);

#endif /* AUDIO_CONFIG_H */
