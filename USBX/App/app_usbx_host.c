/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_usbx_host.c
  * @author  MCD Application Team
  * @brief   USBX host applicative file
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
#include "app_usbx_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "ux_host_audio.h"
#include "app_filex.h"
#include "tad5112.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUDIO_FLAG_SPEAKER_CONNECTED  0x01UL
#define AUDIO_FLAG_SD_READY           0x02UL
#define AUDIO_FLAGS_ALL               (AUDIO_FLAG_SPEAKER_CONNECTED | AUDIO_FLAG_SD_READY)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
static TX_THREAD ux_host_app_thread;

/* USER CODE BEGIN PV */
static UX_HOST_CLASS_AUDIO *audio_speaker = UX_NULL;
static TX_EVENT_FLAGS_GROUP audio_event_flags;

#ifdef AUDIO_OUTPUT_SAI
/* Independent USB audio thread — runs audio_playback_wav_files on the same
   proven path as USB-only mode, completely decoupled from the SAI thread. */
#define USB_AUDIO_THREAD_STACK_SIZE  32768U
static UCHAR     s_usb_audio_stack[USB_AUDIO_THREAD_STACK_SIZE];
static TX_THREAD s_usb_audio_thread;

static VOID usb_audio_thread_entry(ULONG arg)
{
    (void)arg;
    extern FX_MEDIA sdio_disk;
    audio_playback_wav_files(audio_speaker, &sdio_disk);
}
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID app_ux_host_thread_entry(ULONG thread_input);
static UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance);
static VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code);

extern HCD_HandleTypeDef hhcd_USB_DRD_FS;

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  Application USBX Host Initialization.
  * @param  memory_ptr: memory pointer
  * @retval status
  */
UINT MX_USBX_Host_Init(VOID *memory_ptr)
{
  UINT ret = UX_SUCCESS;

  UCHAR *pointer;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN MX_USBX_Host_Init0 */
  /* USER CODE END MX_USBX_Host_Init0 */

  /* USER CODE BEGIN MX_USBX_Host_Init 1 */

  /* USER CODE END MX_USBX_Host_Init 1 */

  /* Allocate the stack for host application main thread */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, UX_HOST_APP_THREAD_STACK_SIZE,
                       TX_NO_WAIT) != TX_SUCCESS)
  {
    /* USER CODE BEGIN MAIN_THREAD_ALLOCATE_STACK_ERROR */
    return TX_POOL_ERROR;
    /* USER CODE END MAIN_THREAD_ALLOCATE_STACK_ERROR */
  }

  /* Create the host application main thread */
  if (tx_thread_create(&ux_host_app_thread, UX_HOST_APP_THREAD_NAME, app_ux_host_thread_entry,
                       0, pointer, UX_HOST_APP_THREAD_STACK_SIZE, UX_HOST_APP_THREAD_PRIO,
                       UX_HOST_APP_THREAD_PREEMPTION_THRESHOLD, UX_HOST_APP_THREAD_TIME_SLICE,
                       UX_HOST_APP_THREAD_START_OPTION) != TX_SUCCESS)
  {
    /* USER CODE BEGIN MAIN_THREAD_CREATE_ERROR */
    return TX_THREAD_ERROR;
    /* USER CODE END MAIN_THREAD_CREATE_ERROR */
  }

  /* USER CODE BEGIN MX_USBX_Host_Init 2 */
  if (tx_event_flags_create(&audio_event_flags, "audio_events") != TX_SUCCESS)
  {
    return TX_GROUP_ERROR;
  }
  /* USER CODE END MX_USBX_Host_Init 2 */

  return ret;
}

/**
  * @brief  Function implementing app_ux_host_thread_entry.
  * @param  thread_input: User thread input parameter.
  * @retval none
  */
static VOID app_ux_host_thread_entry(ULONG thread_input)
{
  /* USER CODE BEGIN app_ux_host_thread_entry */
  TX_PARAMETER_NOT_USED(thread_input);

  MX_USBX_Host_Stack_Init();

  while (1)
  {
    extern TX_EVENT_FLAGS_GROUP sd_event_flags;
    extern FX_MEDIA sdio_disk;

#ifdef AUDIO_OUTPUT_SAI
    /* SAI path: wait for SD (mandatory), then give USB speaker up to 3 s to
       enumerate.  If no speaker is found within the timeout, audio_speaker
       stays NULL and SAI-only mode activates automatically. */
    ULONG sd_flags;
    extern I2C_HandleTypeDef hi2c1;
    tx_event_flags_get(&sd_event_flags, 0x01UL, TX_AND, &sd_flags, TX_WAIT_FOREVER);
    tx_event_flags_get(&audio_event_flags, AUDIO_FLAG_SPEAKER_CONNECTED,
                       TX_AND, &sd_flags, 3 * TX_TIMER_TICKS_PER_SECOND);
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2);
    /* Initialize TAD5112 CODEC via I2C: wake, set I2S+32-bit, enable DAC */
    tad5112_init(&hi2c1);
    /* If a USB speaker is connected, run it on a dedicated thread using the
       same proven path as USB-only mode — completely independent of SAI. */
    if (audio_speaker != UX_NULL)
    {
        tx_thread_create(&s_usb_audio_thread, "usb_audio",
                         usb_audio_thread_entry, 0,
                         s_usb_audio_stack, USB_AUDIO_THREAD_STACK_SIZE,
                         10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);
    }
    /* SAI plays independently — no USB involvement */
    audio_playback_sai_files(&sdio_disk, UX_NULL);
#else
    /* USB path: wait for both USB speaker and SD card */
    ULONG actual_flags;
    ULONG sd_flags;
    tx_event_flags_get(&audio_event_flags, AUDIO_FLAG_SPEAKER_CONNECTED,
                       TX_AND, &actual_flags, TX_WAIT_FOREVER);
    tx_event_flags_get(&sd_event_flags, 0x01UL, TX_AND, &sd_flags, TX_WAIT_FOREVER);
    if (audio_speaker != UX_NULL)
    {
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2);
      audio_playback_wav_files(audio_speaker, &sdio_disk);
    }
#endif
  }
  /* USER CODE END app_ux_host_thread_entry */
}

/**
  * @brief  ux_host_event_callback
  *         This callback is invoked to notify application of instance changes.
  * @param  event: event code.
  * @param  current_class: Pointer to class.
  * @param  current_instance: Pointer to class instance.
  * @retval status
  */
UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance)
{
  UINT status = UX_SUCCESS;

  /* USER CODE BEGIN ux_host_event_callback0 */

  /* USER CODE END ux_host_event_callback0 */

  switch (event)
  {
    case UX_DEVICE_INSERTION:

      /* USER CODE BEGIN UX_DEVICE_INSERTION */
      {
        /* Guard: hub, HID, and other class insertions also fire this callback
           with a different current_instance type.  Only process audio. */
        if (ux_utility_memory_compare(current_class->ux_host_class_name,
                                      _ux_system_host_class_audio_name,
                                      ux_utility_string_length_get(
                                          _ux_system_host_class_audio_name)) != UX_SUCCESS)
            break;

        UX_HOST_CLASS_AUDIO *audio_instance = (UX_HOST_CLASS_AUDIO *)current_instance;

        if (_ux_host_class_audio_subclass_get(audio_instance) ==
            UX_HOST_CLASS_AUDIO_SUBCLASS_STREAMING)
        {
          /* Only accept the first streaming instance — the callback can fire multiple
             times (once per streaming interface: speaker + mic, or per alternate setting).
             Overwriting audio_speaker with a second instance (e.g. a mic or alt-0
             interface) causes a HardFault when stop is called on an instance with no
             active endpoint. */
          if (audio_speaker == UX_NULL)
          {
            UX_ENDPOINT *ep = audio_instance->ux_host_class_audio_isochronous_endpoint;
            /* Accept if ep is NULL (alt setting 0 — real endpoint appears after
               sampling_set) or confirmed OUT (bit7 = 0). Reject confirmed IN (mic). */
            if (ep == UX_NULL ||
                ((ep->ux_endpoint_descriptor.bEndpointAddress & 0x80U) == 0U))
            {
              audio_speaker = audio_instance;
              tx_event_flags_set(&audio_event_flags, AUDIO_FLAG_SPEAKER_CONNECTED, TX_OR);
            }
          }
        }
      }
      /* USER CODE END UX_DEVICE_INSERTION */

      break;

    case UX_DEVICE_REMOVAL:

      /* USER CODE BEGIN UX_DEVICE_REMOVAL */
      if (ux_utility_memory_compare(current_class->ux_host_class_name,
                                    _ux_system_host_class_audio_name,
                                    ux_utility_string_length_get(
                                        _ux_system_host_class_audio_name)) == UX_SUCCESS &&
          (UX_HOST_CLASS_AUDIO *)current_instance == audio_speaker)
      {
        NVIC_SystemReset();
      }
      /* USER CODE END UX_DEVICE_REMOVAL */

      break;

    case UX_DEVICE_CONNECTION:

      /* USER CODE BEGIN UX_DEVICE_CONNECTION */

      /* USER CODE END UX_DEVICE_CONNECTION */

      break;

    case UX_DEVICE_DISCONNECTION:

      /* USER CODE BEGIN UX_DEVICE_DISCONNECTION */

      /* USER CODE END UX_DEVICE_DISCONNECTION */

      break;

    default:

      /* USER CODE BEGIN EVENT_DEFAULT */

      /* USER CODE END EVENT_DEFAULT */

      break;
  }

  /* USER CODE BEGIN ux_host_event_callback1 */

  /* USER CODE END ux_host_event_callback1 */

  return status;
}

/**
  * @brief  ux_host_error_callback
  *         This callback is invoked to notify application of error changes.
  * @param  system_level: system level parameter.
  * @param  system_context: system context code.
  * @param  error_code: error event code.
  * @retval None
  */
VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code)
{
  /* USER CODE BEGIN ux_host_error_callback0 */
  UX_PARAMETER_NOT_USED(system_level);
  UX_PARAMETER_NOT_USED(system_context);
  /* USER CODE END ux_host_error_callback0 */

  switch (error_code)
  {
    case UX_DEVICE_ENUMERATION_FAILURE:

      /* USER CODE BEGIN UX_DEVICE_ENUMERATION_FAILURE */
      audio_playback_usb_disconnect_notify();
      /* USER CODE END UX_DEVICE_ENUMERATION_FAILURE */

      break;

    case  UX_NO_DEVICE_CONNECTED:

      /* USER CODE BEGIN UX_NO_DEVICE_CONNECTED */
      audio_playback_usb_disconnect_notify();
      /* USER CODE END UX_NO_DEVICE_CONNECTED */

      break;

    default:

      /* USER CODE BEGIN ERROR_DEFAULT */

      /* USER CODE END ERROR_DEFAULT */

      break;
  }

  /* USER CODE BEGIN ux_host_error_callback1 */

  /* USER CODE END ux_host_error_callback1 */
}

/**
  * @brief  MX_USBX_Host_Stack_Init
  *         Initialization of USB host stack.
  *         Init USB Host stack, add register the host class stack
  * @retval ret
  */
UINT MX_USBX_Host_Stack_Init(void)
{
  UINT ret = UX_SUCCESS;
  /* USER CODE BEGIN MX_USBX_Host_Stack_Init_PreTreatment_0 */
  /* USER CODE END MX_USBX_Host_Stack_Init_PreTreatment_0 */

  /* The code below is required for installing the host portion of USBX.  */
  if (ux_host_stack_initialize(ux_host_event_callback) != UX_SUCCESS)
  {
    return UX_ERROR;
  }

  /* Register a callback error function */
  ux_utility_error_callback_register(&ux_host_error_callback);
  /* Initialize the host hub class */
  if (ux_host_stack_class_register(_ux_system_host_class_hub_name,
                                   ux_host_class_hub_entry) != UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_HOST_HUB_REGISTER_ERROR */
    return UX_ERROR;
    /* USER CODE END USBX_HOST_HUB_REGISTER_ERROR */
  }

  /* Initialize the host audio class */
  if (ux_host_stack_class_register(_ux_system_host_class_audio_name,
                                   ux_host_class_audio_entry) != UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_HOST_AUDIO_REGISTER_ERROR */
    return UX_ERROR;
    /* USER CODE END USBX_HOST_AUDIO_REGISTER_ERROR */
  }
  /* Register all the USB host controllers available in this system. */
  if (ux_host_stack_hcd_register(_ux_system_host_hcd_stm32_name,
                             _ux_hcd_stm32_initialize, USB_DRD_BASE,
                             (ULONG)&hhcd_USB_DRD_FS)!= UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_HOST_STACK_HCD_REGISTER_ERROR */
    return UX_ERROR;
    /* USER CODE END USBX_HOST_STACK_HCD_REGISTER_ERROR */
  }

  /* Start the USB host controller — drives VBUS, enables SOF and connect interrupts. */
  HAL_HCD_Start(&hhcd_USB_DRD_FS);
  /* USER CODE BEGIN MX_USBX_Host_Stack_Init_PreTreatment_1 */
  /* USER CODE END MX_USBX_Host_Stack_Init_PreTreatment_1 */

  /* USER CODE BEGIN MX_USBX_Host_Stack_Init_PostTreatment */
  /* USER CODE END MX_USBX_Host_Stack_Init_PostTreatment */
  return ret ;
}

/**
  * @brief  MX_USBX_Host_Stack_DeInit
  *         Uninitialize of USB Host stack.
  *         Uninitialize the host stack, unregister of host class stack and
  *         unregister of the usb host controllers
  * @retval ret
  */
UINT MX_USBX_Host_Stack_DeInit(void)
{
  UINT ret = UX_SUCCESS;

  /* USER CODE BEGIN MX_USBX_Host_Stack_DeInit_PreTreatment_0 */

  /* USER CODE END MX_USBX_Host_Stack_DeInit_PreTreatment_0 */

  /* Unregister all the USB host controllers available in this system. */
    if (ux_host_stack_hcd_unregister(_ux_system_host_hcd_stm32_name,
                                USB_DRD_BASE,
                               (ULONG)&hhcd_USB_DRD_FS)!= UX_SUCCESS)
  {
    return UX_ERROR;
  }

  /* Unregister the host hub class */
  if (ux_host_stack_class_unregister(ux_host_class_hub_entry) != UX_SUCCESS)
  {
    return UX_ERROR;
  }

  /* The code below is required for uninstalling the host portion of USBX.  */
  if (ux_host_stack_uninitialize() != UX_SUCCESS)
  {
    return UX_ERROR;
  }

  /* USER CODE BEGIN MX_USBX_Host_Stack_DeInit_PreTreatment_1 */
  /* USER CODE END MX_USBX_Host_Stack_DeInit_PreTreatment_1 */

  /* USER CODE BEGIN MX_USBX_Host_Stack_DeInit_PostTreatment */
  /* USER CODE END MX_USBX_Host_Stack_DeInit_PostTreatment */
  return ret ;
}
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
