#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include "tx_api.h"
#include <stdint.h>

/* Firmware version — shown at boot via LED blink pattern */
#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  4

typedef enum {
    LED_OFF            = 0,
    LED_ON             = 1,
    LED_BLINK_NO_CARD  = 2,  /* 500 ms toggle — no SD card */
    LED_BLINK_NO_AUDIO = 3,  /* 200 ms toggle — card present but no audio files */
} LedState;

/* Call once from tx_application_define (before scheduler). */
void config_led_init(void);

/* Blocking version blink using HAL_Delay — call from main() before tx_kernel_enter on cold boot. */
void led_version_blink(void);

/* Start the LED status timer — always call after led_version_blink (or instead of it). */
void led_timer_start(void);

/* Update LED blink mode. Safe to call from any thread. */
void led_set_state(LedState state);

/* Ready-to-play indicator: PA0 and PA1 solid ON when system is ready. */
void ready_led_set(int on);

/* Volume button LEDs: PA2 = vol-up, PA3 = vol-down.
   Solid ON during playback; blink while button is held; fast-blink when held at the limit.
   Call vol_led_update() every poll tick with current button state and limit flags. */
void vol_led_set_playing(int is_playing);
void vol_led_update(int dn_pressing, int dn_at_limit, int up_pressing, int up_at_limit);

#endif /* AUDIO_CONFIG_H */
