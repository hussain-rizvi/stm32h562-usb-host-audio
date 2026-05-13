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
#include "main.h"
#include "ux_host_audio.h"
#include "audio_config.h"
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
  config_led_init();
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

  config_load_defaults();

  /* SD_EN is on before MX_SDMMC1_SD_Init; wait for CD then mount. Remount on re-insert; unmount on removal
   * only when USB audio is not reading from the card. */
  for (;;)
  {
    led_set_state(LED_BLINK_NO_CARD);
    while (SD_CardIsPresent() == 0U)
    {
      tx_thread_sleep(SD_MOUNT_RETRY_DELAY_TICKS);
    }

    for (;;)
    {
      if (SD_CardIsPresent() == 0U)
      {
        sd_status = FX_IO_ERROR;
        break;
      }
      /* FX_STM32_SD_INIT=0: HAL_SD_Init is the app's responsibility.
         DeInit first to reset handle state (may be HAL_SD_STATE_ERROR from a
         previous failed init when no card was present), then re-init so the
         newly inserted card is fully recognized. */
      extern SD_HandleTypeDef hsd1;
      HAL_SD_DeInit(&hsd1);
      HAL_SD_Init(&hsd1);

      sd_status = fx_media_open(&sdio_disk, FX_SD_VOLUME_NAME, fx_stm32_sd_driver, (VOID *)FX_NULL,
                                (VOID *)fx_sd_media_memory, sizeof(fx_sd_media_memory));
      if (sd_status == FX_SUCCESS)
      {
        break;
      }
      tx_thread_sleep(SD_MOUNT_RETRY_DELAY_TICKS);
    }

    if (sd_status != FX_SUCCESS)
    {
      continue;
    }

    /* Read config.txt from SD root, set TAMP default output if first boot */
    config_read_from_sd(&sdio_disk);
    config_apply_default_output();
    led_set_state(LED_OFF);  /* audio thread sets LED_ON per file, LED_BLINK_NO_AUDIO if none found */

    tx_event_flags_set(&sd_event_flags, SD_FLAG_MEDIA_READY, TX_OR);

    /* EXTI7 on PC7/SD_CD already triggers NVIC_SystemReset on card removal; poll here as a backup. */
    for (;;)
    {
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2);
        if (SD_CardIsPresent() == 0U)
            NVIC_SystemReset();
    }
  }

/* USER CODE END fx_app_thread_entry 0*/

/* Open the SD disk driver */
  sd_status =  fx_media_open(&sdio_disk, FX_SD_VOLUME_NAME, fx_stm32_sd_driver, (VOID *)FX_NULL, (VOID *) fx_sd_media_memory, sizeof(fx_sd_media_memory));

/* Check the media open sd_status */
  if (sd_status != FX_SUCCESS)
  {
     /* USER CODE BEGIN SD DRIVER get info error */
    while(1);
    /* USER CODE END SD DRIVER get info error */
  }

/* USER CODE BEGIN fx_app_thread_entry 1*/

/* USER CODE END fx_app_thread_entry 1*/
  }

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
