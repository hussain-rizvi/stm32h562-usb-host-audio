/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_filex.c
  * @author  MCD Application Team
  * @brief   FileX applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_filex.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tx_api.h"
#include "stm32h5xx_hal_sd.h"
extern SD_HandleTypeDef hsd1;
UINT audio_playback_is_active(VOID);
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* Main thread stack size */
#define FX_APP_THREAD_STACK_SIZE         3072
/* Main thread priority */
#define FX_APP_THREAD_PRIO               10
/* USER CODE BEGIN PD */
/* Delay between SD mount attempts when no card / not ready (allows insert-after-boot). */
#ifndef SD_MOUNT_RETRY_DELAY_TICKS
#define SD_MOUNT_RETRY_DELAY_TICKS  \
    ((TX_TIMER_TICKS_PER_SECOND / 2U) != 0U ? (TX_TIMER_TICKS_PER_SECOND / 2U) : 1U)
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* Main thread global data structures.  */
TX_THREAD       fx_app_thread;

/* Buffer for FileX FX_MEDIA sector cache. */
ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);
/* Define FileX global data structures.  */
FX_MEDIA        sdio_disk;

/* USER CODE BEGIN PV */
TX_EVENT_FLAGS_GROUP sd_event_flags;
#define SD_FLAG_MEDIA_READY  0x01UL
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

/* Main thread entry function.  */
void fx_app_thread_entry(ULONG thread_input);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  Application FileX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
*/
UINT MX_FileX_Init(VOID *memory_ptr)
{
  UINT ret = FX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  VOID *pointer;

/* USER CODE BEGIN MX_FileX_MEM_POOL */

/* USER CODE END MX_FileX_MEM_POOL */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*Allocate memory for the main thread's stack*/
  ret = tx_byte_allocate(byte_pool, &pointer, FX_APP_THREAD_STACK_SIZE, TX_NO_WAIT);

/* Check FX_APP_THREAD_STACK_SIZE allocation*/
  if (ret != FX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

/* Create the main thread.  */
  ret = tx_thread_create(&fx_app_thread, FX_APP_THREAD_NAME, fx_app_thread_entry, 0, pointer, FX_APP_THREAD_STACK_SIZE,
                         FX_APP_THREAD_PRIO, FX_APP_PREEMPTION_THRESHOLD, FX_APP_THREAD_TIME_SLICE, FX_APP_THREAD_AUTO_START);

/* Check main thread creation */
  if (ret != FX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

/* USER CODE BEGIN MX_FileX_Init */
  if (tx_event_flags_create(&sd_event_flags, "sd_events") != TX_SUCCESS)
  {
    return TX_GROUP_ERROR;
  }
/* USER CODE END MX_FileX_Init */

/* Initialize FileX.  */
  fx_system_initialize();

/* USER CODE BEGIN MX_FileX_Init 1*/

/* USER CODE END MX_FileX_Init 1*/

  return ret;
}

/**
 * @brief  Main thread entry.
 * @param thread_input: ULONG user argument used by the thread entry
 * @retval none
*/
 void fx_app_thread_entry(ULONG thread_input)
 {

  UINT sd_status = FX_SUCCESS;

/* USER CODE BEGIN fx_app_thread_entry 0*/
  TX_PARAMETER_NOT_USED(thread_input);

/* USER CODE END fx_app_thread_entry 0*/

  /* Mount → signal ready → on physical removal close media and clear ready (remount), but never
   * call fx_media_close while USB audio is actively reading from SD. */
  for (;;)
  {
    /* Open the SD disk driver — retry until success (no card at boot, or after remove). */
    for (;;)
    {
      sd_status = fx_media_open(&sdio_disk, FX_SD_VOLUME_NAME, fx_stm32_sd_driver, (VOID *)FX_NULL,
                                (VOID *)fx_sd_media_memory, sizeof(fx_sd_media_memory));
      if (sd_status == FX_SUCCESS)
      {
        break;
      }
      tx_thread_sleep(SD_MOUNT_RETRY_DELAY_TICKS);
    }

/* USER CODE BEGIN fx_app_thread_entry 1*/
    tx_event_flags_set(&sd_event_flags, SD_FLAG_MEDIA_READY, TX_OR);

    for (;;)
    {
      tx_thread_sleep(SD_MOUNT_RETRY_DELAY_TICKS);
      if (audio_playback_is_active())
      {
        continue;
      }
      if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_DISCONNECTED)
      {
        (void)fx_media_close(&sdio_disk);
        tx_event_flags_set(&sd_event_flags, (ULONG)(~SD_FLAG_MEDIA_READY), TX_AND);
        break;
      }
    }
/* USER CODE END fx_app_thread_entry 1*/
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
