/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** USBX Component                                                        */
/**                                                                       */
/**   STM32 Controller Driver                                             */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/


/* Include necessary system files.  */

#define UX_SOURCE_CODE
#define UX_HCD_STM32_SOURCE_CODE

#include "ux_api.h"
#include "ux_hcd_stm32.h"
#include "ux_host_stack.h"


/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _ux_hcd_stm32_periodic_schedule                     PORTABLE C      */
/*                                                           6.1.12       */
/*  AUTHOR                                                                */
/*                                                                        */
/*    Chaoqiong Xiao, Microsoft Corporation                               */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*     This function schedules new transfers from the periodic interrupt  */
/*     list.                                                              */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    hcd_stm32                           Pointer to STM32 controller     */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    TRUE or FALSE                                                       */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    HAL_HCD_GetCurrentFrame             Get frame number                */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    STM32 Controller Driver                                             */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  05-19-2020     Chaoqiong Xiao           Initial Version 6.0           */
/*  01-31-2022     Chaoqiong Xiao           Modified comment(s),          */
/*                                            added standalone support,   */
/*                                            resulting in version 6.1.10 */
/*  07-29-2022     Chaoqiong Xiao           Modified comment(s),          */
/*                                            added ISO transfer support, */
/*                                            resulting in version 6.1.12 */
/*                                                                        */
/**************************************************************************/
/* Hub-check block counter: incremented each time the hub port-change check
   sets ep_schedule=0 for an ISO OUT endpoint during normal audio playback.
   Inspect in debugger after ~10 s of hub playback; should stay at 0.
   A rising value means the hub INTR IN has non-zero actual_length and the
   port-change bits show the audio device's port — this blocks ISO OUT frames. */
volatile uint32_t g_ep_schedule_block_count = 0;

/* Set to 1 by the application once the USB speaker is playing audio.
   Suppresses hub INTR IN every frame (NAK-spinning starves ISO OUT → cracking).
   One poll is still allowed every HUB_SLOW_POLL_FRAMES SOFs so the hub class
   can detect speaker removal (→ UX_DEVICE_REMOVAL → NVIC_SystemReset). */
volatile UINT g_hub_poll_stop = 0;
static volatile uint32_t s_hub_slow_count = 0;
#define HUB_SLOW_POLL_FRAMES  20000U  /* ~20 s between removal-detection polls */

/* Diagnostic: counts HAL_HCD_HC_SubmitRequest calls from the slow poll path.
   Should increment roughly every 12 s while audio plays through a hub.
   Stays at 0 → slow poll not reaching submission (sch_mode/interval/state issue). */
volatile uint32_t g_hub_slow_poll_submit_count = 0;

/* Set to 1 just before the slow-poll HAL_HCD_HC_SubmitRequest.  Cleared in the
   callback after the hub INTR IN response is parsed inline (without waking the
   hub class thread, which would run control transfers and cause a crack). */
volatile uint8_t g_hub_slow_poll_active = 0;

/* Deferred hub INTR IN submit: the HAL call is moved to AFTER the while loop so
   the ISO OUT TX FIFO write completes before hub INTR IN is armed.  The NAK IRQ
   from hub INTR IN would otherwise fire mid-FIFO-write and cause a brief underrun. */
static uint8_t              s_hub_submit_after_loop = 0U;
static UX_HCD_STM32_ED     *s_hub_intr_ed           = UX_NULL;


UINT  _ux_hcd_stm32_periodic_schedule(UX_HCD_STM32 *hcd_stm32)
{

UX_HCD_STM32_ED     *ed;
UX_TRANSFER         *transfer_request;
ULONG               frame_index;
UX_DEVICE           *parent_device;
UX_ENDPOINT         *endpoint;
UX_ENDPOINT         *parent_endpoint;
ULONG               ep_schedule = 1U;
USHORT              port_status_change_bits;

    /* Get the current frame number.  */
    frame_index = HAL_HCD_GetCurrentFrame(hcd_stm32 -> hcd_handle);

    /* Get the first ED in the periodic list.  */
    ed =  hcd_stm32 -> ux_hcd_stm32_periodic_ed_head;

    /* Search for an entry in the periodic tree.  */
    while (ed != UX_NULL)
    {
        /* Reset per-ED so one endpoint's hub check cannot block a different
           endpoint's submission in the same scheduler pass. */
        ep_schedule = 1U;

#if defined (USBH_HAL_HUB_SPLIT_SUPPORTED)
      if (hcd_stm32 -> hcd_handle -> hc[ed -> ux_stm32_ed_channel].do_ssplit == 1U)
      {
        /* Get the transfer request.  */
        transfer_request = ed -> ux_stm32_ed_transfer_request;

        if (transfer_request != NULL)
        {
          if ((frame_index & ed -> ux_stm32_ed_interval_mask) == ed -> ux_stm32_ed_interval_position)
          {
            hcd_stm32 -> hcd_handle -> hc[ed -> ux_stm32_ed_channel].ep_ss_schedule = 1U;
          }

          /* Schedule Start & Complete split where the entire split transaction is completely bounded by a
          frame FS/LS devices */
          if (((((frame_index & 0x7U) < 0x3U) || ((frame_index & 0x7U) == 0x7U)) &&
               (hcd_stm32 -> hcd_handle -> hc[ed -> ux_stm32_ed_channel].ep_ss_schedule == 1U)) ||
                ((hcd_stm32 -> hcd_handle -> hc[ed -> ux_stm32_ed_channel].do_csplit == 1U) &&
                 (frame_index > (ed -> ux_stm32_ed_current_ss_frame + 1U))))
          {
            if (hcd_stm32 -> hcd_handle -> hc[ed -> ux_stm32_ed_channel].ep_ss_schedule == 1U)
            {
              hcd_stm32 -> hcd_handle -> hc[ed -> ux_stm32_ed_channel].ep_ss_schedule = 0U;
              ed -> ux_stm32_ed_current_ss_frame = frame_index;
            }

            /* Check if there is transfer needs schedule.  */
            if (ed -> ux_stm32_ed_sch_mode)
            {
              /* If it's scheduled each SOF/uSOF, the request should be submitted
              * immediately after packet is done. This is performed in callback.  */
              if (ed -> ux_stm32_ed_interval_mask == 0U)
                ed -> ux_stm32_ed_sch_mode = 0U;

              /* For ISO OUT, packet size is from request variable,
              * otherwise, use request length.  */
              if ((ed -> ux_stm32_ed_type == EP_TYPE_ISOC) && (ed -> ux_stm32_ed_dir == 0U))
                ed -> ux_stm32_ed_packet_length = transfer_request -> ux_transfer_request_packet_length;
              else
                ed -> ux_stm32_ed_packet_length = transfer_request -> ux_transfer_request_requested_length;

              /* Get the pointer to the Endpoint.  */
              endpoint = (UX_ENDPOINT *) transfer_request -> ux_transfer_request_endpoint;

              /* Check if device connected to hub  */
              if (endpoint->ux_endpoint_device->ux_device_parent != NULL)
              {
                parent_device = endpoint->ux_endpoint_device->ux_device_parent;
                if (parent_device->ux_device_current_configuration->ux_configuration_first_interface->ux_interface_descriptor.bInterfaceClass == 0x9U)
                {
                  parent_endpoint = parent_device->ux_device_current_configuration->ux_configuration_first_interface->ux_interface_first_endpoint;

                  if (parent_endpoint->ux_endpoint_transfer_request.ux_transfer_request_actual_length != 0U)
                  {
                    /* The interrupt pipe buffer contains the status change for each of the ports
                    the length of the buffer can be 1 or 2 depending on the number of ports.
                    Usually, since HUBs can be bus powered the maximum number of ports is 4.
                    We must be taking precautions on how we read the buffer content for
                    big endian machines.  */
                    if (parent_endpoint->ux_endpoint_transfer_request.ux_transfer_request_actual_length == 1U)
                      port_status_change_bits = *(USHORT *) parent_endpoint->ux_endpoint_transfer_request.ux_transfer_request_data_pointer;
                    else
                      port_status_change_bits = (USHORT)_ux_utility_short_get(parent_endpoint->ux_endpoint_transfer_request.ux_transfer_request_data_pointer);

                    if ((port_status_change_bits & (0x1U << endpoint->ux_endpoint_device->ux_device_port_location)) != 0U)
                    {
                      /* ISO OUT is fire-and-forget — do not block on hub port change. */
                    }
                  }
                }
              }

              if ((endpoint->ux_endpoint_device->ux_device_state == UX_DEVICE_CONFIGURED) && (ep_schedule != 0U))
              {
                if (ed -> ux_stm32_ed_data != NULL)
                {
                  ed -> ux_stm32_ed_data_free = UX_HCD_STM32_ED_STATUS_ALIGNED_BUFFER_PENDING_FREE;
                }

                /* Prepare transactions.  */
                _ux_hcd_stm32_request_trans_prepare(hcd_stm32, ed, transfer_request);

                /* Call HAL driver to submit the transfer request.  */
                HAL_HCD_HC_SubmitRequest(hcd_stm32 -> hcd_handle, ed -> ux_stm32_ed_channel,
                                         ed -> ux_stm32_ed_dir,
                                         ed -> ux_stm32_ed_type, USBH_PID_DATA,
                                         ed -> ux_stm32_ed_data + transfer_request -> ux_transfer_request_actual_length,
                                         ed -> ux_stm32_ed_packet_length, 0U);
              }
            }
          }
        }
      }
      else
#endif /* defined (USBH_HAL_HUB_SPLIT_SUPPORTED) */
      {
        UINT do_sched = 1U;

        /* Hub INTR IN NAK-spins inside the USB frame and starves ISO OUT.
           Suppress it while audio plays; allow one poll every ~2 s so the
           hub class can still detect speaker removal → NVIC_SystemReset(). */
        if (g_hub_poll_stop && (ed -> ux_stm32_ed_type == EP_TYPE_INTR) &&
            (ed -> ux_stm32_ed_transfer_request != UX_NULL))
        {
            if (++s_hub_slow_count < HUB_SLOW_POLL_FRAMES)
            {
                ed -> ux_stm32_ed_transfer_request -> ux_transfer_request_actual_length = 0;
                do_sched = 0U;
            }
            else
            {
                /* Counter expired: prepare hub INTR IN but defer the HAL submit
                   to after the while loop.  Submitting inside the loop arms hub
                   INTR IN in the same SOF as ISO OUT; the NAK IRQ fires while the
                   OTG TX FIFO write is in progress → underrun → audible jitter. */
                s_hub_slow_count = 0U;
                transfer_request = ed -> ux_stm32_ed_transfer_request;
                if (transfer_request != UX_NULL)
                {
                    ed -> ux_stm32_ed_packet_length = transfer_request -> ux_transfer_request_requested_length;
                    /* Assign data pointer directly (DMA disabled on STM32H562 FS host,
                       so no aligned-buffer alloc/free needed for hub INTR IN). */
                    ed -> ux_stm32_ed_data = transfer_request -> ux_transfer_request_data_pointer;
                    endpoint = (UX_ENDPOINT *) transfer_request -> ux_transfer_request_endpoint;
                    if (endpoint -> ux_endpoint_device -> ux_device_state == UX_DEVICE_CONFIGURED)
                    {
                        s_hub_intr_ed = ed;
                        s_hub_submit_after_loop = 1U;
                    }
                }
                do_sched = 0U;
            }
        }

        /* Check if the periodic transfer should be scheduled in this frame.  */
        /* Interval Mask is 0:     it's scheduled every SOF/uSOF.  */
        /* Interval Mask is not 0: check position to see if it's scheduled.  */
        if (do_sched &&
            (frame_index & ed -> ux_stm32_ed_interval_mask) == ed -> ux_stm32_ed_interval_position)
        {

          /* Get the transfer request.  */
          transfer_request = ed -> ux_stm32_ed_transfer_request;

          /* Check if there is transfer needs schedule.  */
          if (transfer_request && ed -> ux_stm32_ed_sch_mode)
          {

            /* For ISO OUT, packet size is from request variable,
            * otherwise, use request length.  */
            if ((ed -> ux_stm32_ed_type == EP_TYPE_ISOC) && (ed -> ux_stm32_ed_dir == 0U))
              ed -> ux_stm32_ed_packet_length = transfer_request -> ux_transfer_request_packet_length;
            else
              ed -> ux_stm32_ed_packet_length = transfer_request -> ux_transfer_request_requested_length;

            if (ed -> ux_stm32_ed_data != NULL)
            {
              ed -> ux_stm32_ed_data_free = UX_HCD_STM32_ED_STATUS_ALIGNED_BUFFER_PENDING_FREE;
            }

            /* Prepare transactions.  */
            _ux_hcd_stm32_request_trans_prepare(hcd_stm32, ed, transfer_request);

            /* Get the pointer to the Endpoint.  */
            endpoint = (UX_ENDPOINT *) transfer_request -> ux_transfer_request_endpoint;

            /* Check if device connected to hub  */
            if (endpoint->ux_endpoint_device->ux_device_parent != NULL)
            {
              parent_device = endpoint->ux_endpoint_device->ux_device_parent;
              if (parent_device->ux_device_current_configuration->ux_configuration_first_interface->ux_interface_descriptor.bInterfaceClass == 0x9U)
              {
                parent_endpoint = parent_device->ux_device_current_configuration->ux_configuration_first_interface->ux_interface_first_endpoint;

                if (parent_endpoint->ux_endpoint_transfer_request.ux_transfer_request_actual_length != 0U)
                {
                  /* The interrupt pipe buffer contains the status change for each of the ports
                  the length of the buffer can be 1 or 2 depending on the number of ports.
                  Usually, since HUBs can be bus powered the maximum number of ports is 4.
                  We must be taking precautions on how we read the buffer content for
                  big endian machines.  */
                  if (parent_endpoint->ux_endpoint_transfer_request.ux_transfer_request_actual_length == 1U)
                    port_status_change_bits = *(USHORT *) parent_endpoint->ux_endpoint_transfer_request.ux_transfer_request_data_pointer;
                  else
                    port_status_change_bits = (USHORT)_ux_utility_short_get(parent_endpoint->ux_endpoint_transfer_request.ux_transfer_request_data_pointer);

                  if ((port_status_change_bits & (0x1U << endpoint->ux_endpoint_device->ux_device_port_location)) != 0U)
                  {
                    /* ISO OUT is fire-and-forget — blocking it here causes audible
                       cracking.  Removal is handled via UX_DEVICE_REMOVAL → reset. */
                    g_ep_schedule_block_count++;
                  }
                }
              }
            }

            if ((endpoint->ux_endpoint_device->ux_device_state == UX_DEVICE_CONFIGURED) && (ep_schedule != 0U))
            {
              /* Only clear sch_mode when the submit actually happens.  Clearing it
                 before the hub check meant that if ep_schedule was set to 0 the
                 endpoint was left with sch_mode=0 and no pending hardware request —
                 a deadlock that silently dropped ISO frames each time a song started. */
              if (ed -> ux_stm32_ed_interval_mask == 0U)
                ed -> ux_stm32_ed_sch_mode = 0U;

              /* Call HAL driver to submit the transfer request.  */
              HAL_HCD_HC_SubmitRequest(hcd_stm32 -> hcd_handle, ed -> ux_stm32_ed_channel,
                                       ed -> ux_stm32_ed_dir,
                                       ed -> ux_stm32_ed_type, USBH_PID_DATA,
                                       ed -> ux_stm32_ed_data + transfer_request -> ux_transfer_request_actual_length,
                                       ed -> ux_stm32_ed_packet_length, 0U);
            }
          }
        }
      }

        /* Point to the next ED in the list.  */
        ed =  ed -> ux_stm32_ed_next_ed;
    }

    /* Deferred hub INTR IN submission: ISO OUT FIFO write is now complete for
       this SOF, so arming hub INTR IN here cannot interrupt the write path. */
    if (s_hub_submit_after_loop)
    {
        UX_HCD_STM32_ED *hub_ed = s_hub_intr_ed;
        s_hub_submit_after_loop = 0U;
        s_hub_intr_ed = UX_NULL;
        if (hub_ed != UX_NULL)
        {
            transfer_request = hub_ed -> ux_stm32_ed_transfer_request;
            if (transfer_request != UX_NULL && hub_ed -> ux_stm32_ed_data != UX_NULL)
            {
                g_hub_slow_poll_submit_count++;
                g_hub_slow_poll_active = 1U;
                HAL_HCD_HC_SubmitRequest(hcd_stm32 -> hcd_handle, hub_ed -> ux_stm32_ed_channel,
                                         hub_ed -> ux_stm32_ed_dir,
                                         hub_ed -> ux_stm32_ed_type, USBH_PID_DATA,
                                         hub_ed -> ux_stm32_ed_data,
                                         hub_ed -> ux_stm32_ed_packet_length, 0U);
            }
        }
    }

    /* Return to caller.  */
    return(UX_FALSE);
}

