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
#include "audio_config.h"
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID app_ux_host_thread_entry(ULONG thread_input);
static UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance);
static VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code);

extern HCD_HandleTypeDef hhcd_USB_DRD_FS;

/* USER CODE BEGIN PFP */

/* Walk all alternate settings of the streaming interface (alt-0 has no endpoint).
   The first non-zero alternate setting's endpoint direction tells us speaker vs mic.
   This must be checked here — at enumeration time the active alt is 0, so
   ux_host_class_audio_isochronous_endpoint is NULL for both speaker and mic. */
static int audio_streaming_is_speaker(UX_HOST_CLASS_AUDIO *audio)
{
    UCHAR intf_num = audio->ux_host_class_audio_streaming_interface
                          ->ux_interface_descriptor.bInterfaceNumber;
    UX_INTERFACE *alt = audio->ux_host_class_audio_streaming_interface;
    while (alt != UX_NULL)
    {
        if (alt->ux_interface_descriptor.bInterfaceNumber == intf_num &&
            alt->ux_interface_descriptor.bAlternateSetting != 0U &&
            alt->ux_interface_first_endpoint != UX_NULL)
        {
            /* OUT endpoint (bit7 = 0) → speaker; IN endpoint (bit7 = 1) → mic */
            return (alt->ux_interface_first_endpoint
                       ->ux_endpoint_descriptor.bEndpointAddress & 0x80U) == 0U;
        }
        alt = alt->ux_interface_next_interface;
    }
    return 0; /* no usable alt setting found — reject */
}

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

  led_timer_start();

  /* Wait for FileX to mount SD and populate g_config from config.txt (or defaults).
     Output mode, volume, and range are all valid once this flag is set. */
  {
    extern TX_EVENT_FLAGS_GROUP sd_event_flags;
    ULONG _flags;
    tx_event_flags_get(&sd_event_flags, 0x01UL, TX_AND, &_flags, TX_WAIT_FOREVER);
  }

  /* Only initialise the USB host stack when USB output is selected (config.txt defaultOutput=USB).
     SAI/Analog mode never needs VBUS, HCD, or ISO transfers. */
  const int use_usb_output = (g_config.default_output == 0U);
  if (use_usb_output)
    MX_USBX_Host_Stack_Init();

  while (1)
  {
    extern FX_MEDIA sdio_disk;

    if (!use_usb_output)
    {
      extern I2C_HandleTypeDef hi2c1;
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2);
      tad5112_init(&hi2c1);
      tad5112_set_vol(config_vol_to_tad());
      ready_led_set(1);
      audio_playback_sai_files(&sdio_disk, UX_NULL);
    }
    else
    {
      ULONG actual_flags;
      tx_event_flags_get(&audio_event_flags, AUDIO_FLAG_SPEAKER_CONNECTED,
                         TX_AND, &actual_flags, TX_WAIT_FOREVER);
      if (audio_speaker != UX_NULL)
      {
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2);
        ready_led_set(1);
        audio_playback_wav_files(audio_speaker, &sdio_disk);
      }
    }
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
  UX_PARAMETER_NOT_USED(current_class);
  /* USER CODE END ux_host_event_callback0 */

  switch (event)
  {
    case UX_DEVICE_INSERTION:

      /* USER CODE BEGIN UX_DEVICE_INSERTION */
      {
        UX_HOST_CLASS_AUDIO *audio_instance = (UX_HOST_CLASS_AUDIO *)current_instance;

        if (_ux_host_class_audio_subclass_get(audio_instance) ==
            UX_HOST_CLASS_AUDIO_SUBCLASS_STREAMING)
        {
          /* Devices with mic+speaker+HID fire this callback once per streaming
             interface.  At alt-0 the isochronous_endpoint is always NULL for both
             speaker and mic, so we cannot use it to filter direction here.
             Instead walk the non-zero alternate settings to find the endpoint
             direction — OUT (bit7=0) = speaker, IN (bit7=1) = mic/capture.
             Accept only the first OUT streaming interface. */
          if (audio_speaker == UX_NULL && audio_streaming_is_speaker(audio_instance))
          {
            audio_speaker = audio_instance;
            tx_event_flags_set(&audio_event_flags, AUDIO_FLAG_SPEAKER_CONNECTED, TX_OR);
          }
        }
      }
      /* USER CODE END UX_DEVICE_INSERTION */

      break;

    case UX_DEVICE_REMOVAL:

      /* USER CODE BEGIN UX_DEVICE_REMOVAL */
      if ((UX_HOST_CLASS_AUDIO *)current_instance == audio_speaker)
      {
        system_soft_reset();
      }
      /* USER CODE END UX_DEVICE_REMOVAL */

      break;

    case UX_DEVICE_CONNECTION:

      /* USER CODE BEGIN UX_DEVICE_CONNECTION */

      /* USER CODE END UX_DEVICE_CONNECTION */

      break;

    case UX_DEVICE_DISCONNECTION:

      /* USER CODE BEGIN UX_DEVICE_DISCONNECTION */
      /* If enumeration had failed (audio_speaker never set), clear the fast-blink
         so the system returns to a neutral state ready for the next plug-in. */
      if (audio_speaker == UX_NULL)
          led_set_state(LED_OFF);
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
      /* A USB device was plugged in but USBX found no matching class driver
         for any of its interfaces (e.g. a non-audio device, or a damaged
         descriptor).  Blink fast so the user knows the device was rejected. */
      led_set_state(LED_BLINK_ENUM_FAIL);
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
