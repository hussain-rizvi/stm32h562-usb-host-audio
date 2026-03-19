/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_host_audio.c
  * @author  MCD Application Team
  * @brief   USBX Host Audio applicative source file
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

/* Includes ------------------------------------------------------------------*/
#include "ux_host_audio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tx_api.h"
#include "fx_api.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
    ULONG sample_rate;
    USHORT channels;
    USHORT bits_per_sample;
    ULONG data_size;
} WAV_INFO;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static TX_SEMAPHORE audio_xfer_semaphore;

static VOID audio_transfer_complete(UX_HOST_CLASS_AUDIO_TRANSFER_REQUEST *xfer)
{
    tx_semaphore_put(&audio_xfer_semaphore);
}

static UINT wav_parse_header(FX_FILE *file, WAV_INFO *info)
{
    UCHAR hdr[12];
    ULONG bytes_read;
    UINT status;
    ULONG riff_end;

    status = fx_file_seek(file, 0);
    if (status != FX_SUCCESS)
        return status;

    status = fx_file_read(file, hdr, 12, &bytes_read);
    if (status != FX_SUCCESS || bytes_read < 12)
        return FX_IO_ERROR;

    if (hdr[0] != 'R' || hdr[1] != 'I' || hdr[2] != 'F' || hdr[3] != 'F')
        return FX_IO_ERROR;

    riff_end = 8 + (ULONG)(hdr[4] | (hdr[5] << 8) | (hdr[6] << 16) | (hdr[7] << 24));

    if (hdr[8] != 'W' || hdr[9] != 'A' || hdr[10] != 'V' || hdr[11] != 'E')
        return FX_IO_ERROR;

    info->channels = 0;
    info->sample_rate = 0;
    info->bits_per_sample = 0;
    info->data_size = 0;

    ULONG pos = 12;
    while (pos + 8 <= riff_end)
    {
        UCHAR chunk_hdr[8];

        status = fx_file_seek(file, pos);
        if (status != FX_SUCCESS)
            return status;

        status = fx_file_read(file, chunk_hdr, 8, &bytes_read);
        if (status != FX_SUCCESS || bytes_read < 8)
            return FX_IO_ERROR;

        ULONG chunk_size = (ULONG)(chunk_hdr[4] | (chunk_hdr[5] << 8) |
                                   (chunk_hdr[6] << 16) | (chunk_hdr[7] << 24));

        if (chunk_hdr[0] == 'f' && chunk_hdr[1] == 'm' &&
            chunk_hdr[2] == 't' && chunk_hdr[3] == ' ')
        {
            UCHAR fmt[16];
            status = fx_file_read(file, fmt, 16, &bytes_read);
            if (status != FX_SUCCESS || bytes_read < 16)
                return FX_IO_ERROR;

            USHORT audio_format = (USHORT)(fmt[0] | (fmt[1] << 8));
            if (audio_format != 1)
                return FX_IO_ERROR;

            info->channels        = (USHORT)(fmt[2] | (fmt[3] << 8));
            info->sample_rate     = (ULONG)(fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | (fmt[7] << 24));
            info->bits_per_sample = (USHORT)(fmt[14] | (fmt[15] << 8));
        }
        else if (chunk_hdr[0] == 'd' && chunk_hdr[1] == 'a' &&
                 chunk_hdr[2] == 't' && chunk_hdr[3] == 'a')
        {
            info->data_size = chunk_size;
            if (info->channels == 0)
                return FX_IO_ERROR;
            return FX_SUCCESS;
        }

        pos += 8 + chunk_size;
        if (chunk_size & 1)
            pos++;
    }

    return FX_IO_ERROR;
}

static UINT audio_stream_wav(UX_HOST_CLASS_AUDIO *audio, FX_FILE *file, WAV_INFO *info)
{
    UINT status;
    ULONG packet_size;
    ULONG remaining;
    ULONG bytes_read;
    ULONG to_read;
    UINT fill_idx;
    INT in_flight_idx;
    UX_HOST_CLASS_AUDIO_SAMPLING audio_sampling;

    static UCHAR audio_buffer[2][AUDIO_MAX_PACKET_SIZE] __attribute__((aligned(4)));
    static UX_HOST_CLASS_AUDIO_TRANSFER_REQUEST audio_xfer[2];

    audio_sampling.ux_host_class_audio_sampling_channels   = info->channels;
    audio_sampling.ux_host_class_audio_sampling_frequency   = info->sample_rate;
    audio_sampling.ux_host_class_audio_sampling_resolution  = info->bits_per_sample;

    status = ux_host_class_audio_streaming_sampling_set(audio, &audio_sampling);
    if (status != UX_SUCCESS)
        return status;

    packet_size = ux_host_class_audio_packet_size_get(audio);
    if (packet_size == 0 || packet_size > AUDIO_MAX_PACKET_SIZE)
        packet_size = AUDIO_MAX_PACKET_SIZE;

    remaining = info->data_size;
    fill_idx = 0;
    in_flight_idx = -1;

    while (remaining > 0)
    {
        to_read = (remaining > packet_size) ? packet_size : remaining;

        status = fx_file_read(file, audio_buffer[fill_idx], to_read, &bytes_read);
        if (status != FX_SUCCESS || bytes_read == 0)
            break;

        if (bytes_read < packet_size)
        {
            ULONG i;
            for (i = bytes_read; i < packet_size; i++)
                audio_buffer[fill_idx][i] = 0;
            bytes_read = packet_size;
        }

        if (in_flight_idx >= 0)
        {
            status = tx_semaphore_get(&audio_xfer_semaphore, TX_WAIT_FOREVER);
            if (status != TX_SUCCESS)
                return UX_ERROR;
        }

        audio_xfer[fill_idx].ux_host_class_audio_transfer_request_data_pointer = audio_buffer[fill_idx];
        audio_xfer[fill_idx].ux_host_class_audio_transfer_request_requested_length = bytes_read;
        audio_xfer[fill_idx].ux_host_class_audio_transfer_request_packet_size = packet_size;
        audio_xfer[fill_idx].ux_host_class_audio_transfer_request_completion_function = audio_transfer_complete;

        status = ux_host_class_audio_write(audio, &audio_xfer[fill_idx]);
        if (status != UX_SUCCESS)
            return status;

        in_flight_idx = (INT)fill_idx;
        fill_idx = 1U - fill_idx;
        remaining -= (to_read <= remaining) ? to_read : remaining;
    }

    if (in_flight_idx >= 0)
    {
        status = tx_semaphore_get(&audio_xfer_semaphore, TX_WAIT_FOREVER);
        if (status != TX_SUCCESS)
            return UX_ERROR;
    }

    return UX_SUCCESS;
}

static UINT str_ends_with_wav(CHAR *name)
{
    UINT len = 0;
    while (name[len] != '\0')
        len++;
    if (len < 4)
        return 0;
    return ((name[len-4] == '.' || name[len-4] == '.') &&
            (name[len-3] == 'w' || name[len-3] == 'W') &&
            (name[len-2] == 'a' || name[len-2] == 'A') &&
            (name[len-1] == 'v' || name[len-1] == 'V'));
}

/* USER CODE END 0 */

/* USER CODE BEGIN 1 */

VOID audio_playback_wav_files(UX_HOST_CLASS_AUDIO *audio, FX_MEDIA *media)
{
    UINT status;
    CHAR entry_name[FX_MAX_LONG_NAME_LEN];
    UINT file_attributes;
    ULONG file_size;
    FX_FILE wav_file;
    WAV_INFO wav_info;

    tx_semaphore_create(&audio_xfer_semaphore, "audio_xfer_sem", 0);

    while (1)
    {
        status = fx_directory_first_entry_find(media, entry_name);

        while (status == FX_SUCCESS)
        {
            fx_directory_information_get(media, entry_name, &file_attributes,
                                         &file_size, UX_NULL, UX_NULL,
                                         UX_NULL, UX_NULL, UX_NULL, UX_NULL);

            if (!(file_attributes & FX_DIRECTORY) && str_ends_with_wav(entry_name))
            {
                status = fx_file_open(media, &wav_file, entry_name, FX_OPEN_FOR_READ);
                if (status == FX_SUCCESS)
                {
                    status = wav_parse_header(&wav_file, &wav_info);
                    if (status == FX_SUCCESS)
                    {
                        status = audio_stream_wav(audio, &wav_file, &wav_info);
                    }
                    fx_file_close(&wav_file);

                    if (status != FX_SUCCESS && status != UX_SUCCESS)
                        goto done;
                }
            }

            status = fx_directory_next_entry_find(media, entry_name);
        }
    }

done:
    ux_host_class_audio_stop(audio);
    tx_semaphore_delete(&audio_xfer_semaphore);
}

/* USER CODE END 1 */
