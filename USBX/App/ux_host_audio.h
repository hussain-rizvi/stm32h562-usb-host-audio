/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_host_audio.h
  * @author  MCD Application Team
  * @brief   USBX Host Audio applicative header file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __UX_HOST_AUDIO_H__
#define __UX_HOST_AUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ux_api.h"
#include "ux_host_class_audio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "fx_api.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/

/* USER CODE BEGIN EFP */
VOID audio_playback_wav_files(UX_HOST_CLASS_AUDIO *audio, FX_MEDIA *media);
/* Call from USB host removal path so playback exits waits promptly (plug/unplug safe). */
VOID audio_playback_usb_disconnect_notify(VOID);
/* TX_TRUE while audio_playback_wav_files() is using the SD media (defer remount/close). */
UINT audio_playback_is_active(VOID);
/* Skip to the next track. Safe to call from ISR. */
VOID audio_skip_track(VOID);
/* Toggle play/pause. Safe to call from ISR. */
VOID audio_play_pause(VOID);
#ifdef AUDIO_OUTPUT_SAI
/* SAI analogue output path — plays WAV/MP3 from SD card through the on-board I2S DAC.
   Pass usb_audio != NULL to simultaneously mirror audio to a USB speaker (dual output).
   Pass UX_NULL for SAI-only mode. */
VOID audio_playback_sai_files(FX_MEDIA *media, UX_HOST_CLASS_AUDIO *usb_audio);
#endif
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUDIO_MAX_PACKET_SIZE   256
/* USER CODE END PD */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif  /* __UX_HOST_AUDIO_H__ */
