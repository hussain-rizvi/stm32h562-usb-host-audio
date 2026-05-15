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
#include "main.h"
#include "tad5112.h"
#include "tx_api.h"
#include "fx_api.h"
/* minimp3 inner decode loops (MDCT/QMF) must run fast regardless of project
   build config.  -O0 (Debug) makes one frame take ~80 ms on Cortex-M33 —
   far longer than the 26 ms frame duration — causing guaranteed underrun.
   -Os (Release) sacrifices throughput for size on exactly the wrong code.
   Force -O2 for this translation unit only. */
#pragma GCC optimize("O2")
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"
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
#define AUDIO_XFER_SEMAPHORE_TIMEOUT  (10 * TX_TIMER_TICKS_PER_SECOND)  /* 10 seconds */
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

/* Set by USB removal / error path; playback thread polls this so unplug does not wait full ISO timeout. */
static volatile UINT s_audio_playback_abort;

/* Consecutive ISO OUT errors — reset board when this crosses the threshold.
   Detects speaker removal when hub interrupt polling is stopped (no hub class
   removal event) by watching for a burst of URB_ERROR completions. */
#define AUDIO_ISO_ERROR_RESET_THRESHOLD  20
static volatile UINT s_iso_consecutive_errors = 0;

/* ISO completion diagnostics — inspect in debugger after ~10 s of playback.
   g_iso_complete_count should grow at ~1000/sec (one per USB frame).
   g_iso_stall_50ms_count > 0 means the ISO pipeline stalled (no completions for 50 ms).
   g_iso_error_count: completions with non-UX_SUCCESS code (signal integrity / dropped ISO). */
volatile uint32_t g_iso_complete_count = 0;
volatile uint32_t g_iso_stall_50ms_count = 0;
volatile uint32_t g_iso_error_count = 0;
/* 0 = PLL3Q active, 1 = PLL3Q config failed (HAL_ERROR), 2 = timeout, 3 = busy, 0xFF = not attempted */
volatile uint32_t g_usb_clock_source = 0xFF;

/* Hub-specific diagnostics (hub connection only).
   g_ep_schedule_block_count: SOF frames where the hub port-change check blocked
     ISO OUT — should be 0 during normal playback (hub actual_length == 0).
   g_iso_cb_nonzero_actual: times the callback-path ISO OUT submit had non-zero
     actual_length — should be 0 after the actual_length fix is in place. */
extern volatile uint32_t g_ep_schedule_block_count;
extern volatile uint32_t g_iso_cb_nonzero_actual;

/* Separate active flags so SAI and USB threads can run concurrently without
   one clearing the other's flag and allowing FileX to close the media early. */
static volatile UINT s_audio_playback_active;      /* SAI path */
static volatile UINT s_audio_playback_active_usb;  /* USB path */

/*
 * Block until ISO transfer completes or disconnect/timeout.
 * Uses a short blocking wait (not poll+sleep) so the thread is rescheduled
 * immediately when the USB completion callback fires the semaphore, instead
 * of waking up 1 tick too late and missing the next USB frame.
 */
#define AUDIO_XFER_WAIT_TICKS  (TX_TIMER_TICKS_PER_SECOND / 20)  /* 50 ms max block per check */
static UINT audio_wait_xfer_done(VOID)
{
    ULONG t0 = tx_time_get();
    while (!s_audio_playback_abort)
    {
        if (tx_semaphore_get(&audio_xfer_semaphore, AUDIO_XFER_WAIT_TICKS) == TX_SUCCESS)
            return UX_SUCCESS;
        g_iso_stall_50ms_count++;
        if ((tx_time_get() - t0) >= AUDIO_XFER_SEMAPHORE_TIMEOUT)
            return UX_ERROR;
    }
    return UX_ERROR;
}

/*
 * SD drain buffer — read SD card in 4 KB chunks and serve packet-sized
 * requests from RAM.  A single 4 KB SD read covers ~23 USB frames at
 * 44.1 kHz stereo 16-bit (176 bytes/frame), so FileX overhead is paid
 * once every ~23 ms instead of once per 1 ms USB frame.
 */
#define AUDIO_SD_CHUNK_SIZE  4096U
static UCHAR s_sd_chunk_buf[AUDIO_SD_CHUNK_SIZE] __attribute__((aligned(4)));
static ULONG s_sd_chunk_pos;
static ULONG s_sd_chunk_avail;

static VOID drain_reset(VOID)
{
    s_sd_chunk_pos   = 0;
    s_sd_chunk_avail = 0;
}

static UINT drain_read(FX_FILE *f, UCHAR *dst, ULONG want, ULONG *got_out)
{
    ULONG got = 0;
    while (got < want)
    {
        if (s_sd_chunk_avail == 0)
        {
            ULONG nr;
            UINT st = fx_file_read(f, s_sd_chunk_buf, AUDIO_SD_CHUNK_SIZE, &nr);
            if (st != FX_SUCCESS || nr == 0)
                break;
            s_sd_chunk_pos   = 0;
            s_sd_chunk_avail = nr;
        }
        ULONG take = want - got;
        if (take > s_sd_chunk_avail)
            take = s_sd_chunk_avail;
        memcpy(dst + got, s_sd_chunk_buf + s_sd_chunk_pos, take);
        s_sd_chunk_pos   += take;
        s_sd_chunk_avail -= take;
        got              += take;
    }
    *got_out = got;
    return (got > 0) ? FX_SUCCESS : FX_IO_ERROR;
}

VOID audio_playback_usb_disconnect_notify(VOID)
{
    s_audio_playback_abort = TX_TRUE;
}

UINT audio_playback_is_active(VOID)
{
    return s_audio_playback_active || s_audio_playback_active_usb;
}

/* MP3 decode uses large stack inside minimp3 (~15 KB). Keep I/O buffers & decoder out of thread stack. */
#define MP3_READ_BUF_SIZE  4096
static mp3dec_t s_mp3_decoder;
static UCHAR s_mp3_read_buf[MP3_READ_BUF_SIZE];
static mp3d_sample_t s_mp3_pcm_buf[MINIMP3_MAX_SAMPLES_PER_FRAME];

static VOID audio_transfer_complete(UX_HOST_CLASS_AUDIO_TRANSFER_REQUEST *xfer)
{
    g_iso_complete_count++;
    if (xfer->ux_host_class_audio_transfer_request_completion_code != UX_SUCCESS)
    {
        g_iso_error_count++;
        if (++s_iso_consecutive_errors >= AUDIO_ISO_ERROR_RESET_THRESHOLD)
            NVIC_SystemReset();
    }
    else
    {
        s_iso_consecutive_errors = 0;
    }
    tx_semaphore_put(&audio_xfer_semaphore);
}

/* ===================================================================
 * SAI analogue output path  (32-bit I2S → TAD5112 CODEC)
 * =================================================================== */
#ifdef AUDIO_OUTPUT_SAI

extern SAI_HandleTypeDef hsai_BlockA1;
extern DMA_HandleTypeDef hdma_sai1_a;

/* DMA ping-pong: two halves, each SAI_HALF_SAMPLES stereo 32-bit samples.
   SAI is configured as SAI_PROTOCOL_DATASIZE_32BIT → BCLK/FSYNC = 64.
   16-bit PCM from WAV/MP3 is left-shifted into the MSB of each 32-bit word
   so the TAD5112 (32-bit mode) sees the correct amplitude.
   Lower half = sai_dma_buf[0 .. SAI_HALF_SAMPLES*2-1]
   Upper half = sai_dma_buf[SAI_HALF_SAMPLES*2 .. SAI_HALF_SAMPLES*4-1] */
#define SAI_HALF_SAMPLES  1024U
static int32_t sai_dma_buf[SAI_HALF_SAMPLES * 4U] __attribute__((aligned(32)));

/* Temporary staging buffer: 16-bit PCM read from SD before 32-bit expansion */
static int16_t s_sai_pcm16_tmp[SAI_HALF_SAMPLES * 2U];
/* Staging buffer for MP3→USB 32→16-bit conversion in dual-output mode */
static int16_t s_usb_dual_pcm16[SAI_HALF_SAMPLES * 2U];

static TX_SEMAPHORE   sai_sem;
static volatile UINT  sai_fill_upper;  /* 0 = lower half free, 1 = upper half free */

/* SAI-specific SD drain buffer — completely separate from the USB drain buffer
   so both threads can stream SD concurrently without corrupting each other. */
static UCHAR s_sai_sd_chunk_buf[AUDIO_SD_CHUNK_SIZE] __attribute__((aligned(4)));
static ULONG s_sai_sd_chunk_pos;
static ULONG s_sai_sd_chunk_avail;

static VOID sai_drain_reset(VOID)
{
    s_sai_sd_chunk_pos   = 0;
    s_sai_sd_chunk_avail = 0;
}

static UINT sai_drain_read(FX_FILE *f, UCHAR *dst, ULONG want, ULONG *got_out)
{
    ULONG got = 0;
    while (got < want)
    {
        if (s_sai_sd_chunk_avail == 0)
        {
            ULONG nr;
            UINT st = fx_file_read(f, s_sai_sd_chunk_buf, AUDIO_SD_CHUNK_SIZE, &nr);
            if (st != FX_SUCCESS || nr == 0)
                break;
            s_sai_sd_chunk_pos   = 0;
            s_sai_sd_chunk_avail = nr;
        }
        ULONG take = want - got;
        if (take > s_sai_sd_chunk_avail)
            take = s_sai_sd_chunk_avail;
        memcpy(dst + got, s_sai_sd_chunk_buf + s_sai_sd_chunk_pos, take);
        s_sai_sd_chunk_pos   += take;
        s_sai_sd_chunk_avail -= take;
        got                  += take;
    }
    *got_out = got;
    return (got > 0) ? FX_SUCCESS : FX_IO_ERROR;
}

/* SAI-specific MP3 state — separate from USB path so both can decode independently */
static mp3dec_t      s_sai_mp3_decoder;
static UCHAR         s_sai_mp3_read_buf[MP3_READ_BUF_SIZE];
static mp3d_sample_t s_sai_mp3_pcm_buf[MINIMP3_MAX_SAMPLES_PER_FRAME];

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    (void)hsai;
    sai_fill_upper = 0U;
    tx_semaphore_put(&sai_sem);
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
    (void)hsai;
    sai_fill_upper = 1U;
    tx_semaphore_put(&sai_sem);
}

static VOID sai_sem_flush(VOID)
{
    while (tx_semaphore_get(&sai_sem, TX_NO_WAIT) == TX_SUCCESS) {}
}

/* Dual-output state — when s_usb_audio != NULL, PCM16 is also sent to USB speaker */
static UX_HOST_CLASS_AUDIO *s_usb_audio          = UX_NULL;
static ULONG                s_usb_pkt_size       = 0;
static ULONG                s_usb_sample_rate    = 0;
static UINT                 s_usb_channels_num   = 0;
static ULONG                s_usb_frac_acc       = 0;
static volatile UINT        s_usb_running        = 0;
static UINT                 s_usb_thread_active  = 0;

/* PCM FIFO — SAI thread writes, USB feeder thread reads.
   Single-producer / single-consumer lock-free ring.  Power-of-2 for mask wrapping. */
#define USB_PCM_FIFO_SIZE  16384U
static UCHAR          s_usb_fifo[USB_PCM_FIFO_SIZE];
static volatile ULONG s_usb_fifo_wr = 0;
static volatile ULONG s_usb_fifo_rd = 0;

/* USB feeder thread — dedicated thread that submits USB packets independently of SAI */
#define USB_FEEDER_STACK_SIZE  4096U
static UCHAR        s_usb_stack[USB_FEEDER_STACK_SIZE];
static TX_THREAD    s_usb_thread;
static TX_SEMAPHORE s_usb_xfer_sem;  /* signalled by USB completion callback */
static TX_SEMAPHORE s_usb_done_sem;  /* signalled by feeder thread on exit */

static VOID usb_fifo_reset(VOID)
{
    s_usb_fifo_wr = 0;
    s_usb_fifo_rd = 0;
}

/* SAI thread writes PCM here — non-blocking, drops if full */
static ULONG usb_fifo_write(const UCHAR *src, ULONG n)
{
    ULONG free = USB_PCM_FIFO_SIZE - 1U -
                 ((s_usb_fifo_wr - s_usb_fifo_rd) & (USB_PCM_FIFO_SIZE - 1U));
    if (n > free) n = free;
    for (ULONG i = 0; i < n; i++)
        s_usb_fifo[(s_usb_fifo_wr + i) & (USB_PCM_FIFO_SIZE - 1U)] = src[i];
    __DMB();
    s_usb_fifo_wr += n;
    return n;
}

/* USB feeder thread reads PCM here — zero-pads if FIFO runs low */
static VOID usb_fifo_read_padded(UCHAR *dst, ULONG n)
{
    ULONG avail = (s_usb_fifo_wr - s_usb_fifo_rd) & (USB_PCM_FIFO_SIZE - 1U);
    ULONG take  = (avail < n) ? avail : n;
    for (ULONG i = 0; i < take; i++)
        dst[i] = s_usb_fifo[(s_usb_fifo_rd + i) & (USB_PCM_FIFO_SIZE - 1U)];
    if (take < n)
        memset(dst + take, 0, n - take);
    __DMB();
    s_usb_fifo_rd += take;
}

/* Forward declarations — bodies are after the ring buffer arrays */
static VOID sai_submit_pcm16_to_usb(const int16_t *pcm16, ULONG n_bytes);
static VOID sai_usb_prefill(VOID);
static VOID sai_usb_drain(VOID);
static VOID sai_usb_configure(ULONG sample_rate, UINT channels);

/* Two PLL2 FRACN values — M=1 N=33 P=2 VCIRANGE_3 (VCI=8 MHz, VCO≈270 MHz):
   FRACN=7117 → PLL2P=135,475,097 Hz: 44100 → 44099.97 Hz (−0.000075%)
   FRACN=6488 → PLL2P=135,167,969 Hz: 48000 → 47999.99 Hz (−0.000025%)
                                        32000 → 31999.99 Hz (−0.000025%, NODIV) */
#define PLL2_FRACN_44K  7117U
#define PLL2_FRACN_48K  6488U

static uint32_t s_pll2_fracn = PLL2_FRACN_48K;  /* matches startup init (48k default) */

static HAL_StatusTypeDef pll2_set_fracn(uint32_t fracn)
{
    if (s_pll2_fracn == fracn)
        return HAL_OK;
    RCC_PeriphCLKInitTypeDef clk = {0};
    clk.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
    clk.PLL2.PLL2Source      = RCC_PLL2_SOURCE_HSE;
    clk.PLL2.PLL2M           = 1;
    clk.PLL2.PLL2N           = 33;
    clk.PLL2.PLL2P           = 2;
    clk.PLL2.PLL2Q           = 2;
    clk.PLL2.PLL2R           = 2;
    clk.PLL2.PLL2RGE         = RCC_PLL2_VCIRANGE_3;
    clk.PLL2.PLL2VCOSEL      = RCC_PLL2_VCORANGE_WIDE;
    clk.PLL2.PLL2FRACN       = fracn;
    clk.PLL2.PLL2ClockOut    = RCC_PLL2_DIVP;
    clk.Sai1ClockSelection   = RCC_SAI1CLKSOURCE_PLL2P;
    HAL_StatusTypeDef st = HAL_RCCEx_PeriphCLKConfig(&clk);
    if (st == HAL_OK)
        s_pll2_fracn = fracn;
    return st;
}

/* Reconfigure PLL2 FRACN and SAI MCLKDIV for the given sample rate.
   Called between files; SAI must not be running (DMAStop before this). */
static HAL_StatusTypeDef sai_reconfigure(ULONG sample_rate)
{
    uint32_t freq;
    uint32_t nodiv;
    uint32_t fracn;
    switch (sample_rate)
    {
        case  8000U: freq = SAI_AUDIO_FREQUENCY_8K;  nodiv = SAI_MASTERDIVIDER_ENABLE;  fracn = PLL2_FRACN_48K; break;
        case 11025U: freq = SAI_AUDIO_FREQUENCY_11K; nodiv = SAI_MASTERDIVIDER_ENABLE;  fracn = PLL2_FRACN_44K; break;
        case 16000U: freq = SAI_AUDIO_FREQUENCY_16K; nodiv = SAI_MASTERDIVIDER_ENABLE;  fracn = PLL2_FRACN_48K; break;
        case 22050U: freq = SAI_AUDIO_FREQUENCY_22K; nodiv = SAI_MASTERDIVIDER_ENABLE;  fracn = PLL2_FRACN_44K; break;
        case 32000U: freq = SAI_AUDIO_FREQUENCY_32K; nodiv = SAI_MASTERDIVIDER_DISABLE; fracn = PLL2_FRACN_48K; break;
        case 44100U: freq = SAI_AUDIO_FREQUENCY_44K; nodiv = SAI_MASTERDIVIDER_ENABLE;  fracn = PLL2_FRACN_44K; break;
        default:     freq = SAI_AUDIO_FREQUENCY_48K; nodiv = SAI_MASTERDIVIDER_ENABLE;  fracn = PLL2_FRACN_48K; break;
    }
    UINT pll_changed = (s_pll2_fracn != fracn);
    HAL_StatusTypeDef st = pll2_set_fracn(fracn);
    if (st != HAL_OK)
        return st;
    /* Reinit SAI whenever PLL changed — MCKDIV must be recomputed against new PLL2P */
    if (pll_changed ||
        hsai_BlockA1.Init.AudioFrequency != freq ||
        hsai_BlockA1.Init.NoDivider      != nodiv)
    {
        hsai_BlockA1.Init.AudioFrequency = freq;
        hsai_BlockA1.Init.NoDivider      = nodiv;
        st = HAL_SAI_Init(&hsai_BlockA1);
        if (st != HAL_OK)
            return st;
    }
    return HAL_OK;
}

/* Expand count 16-bit PCM samples into 32-bit MSB-aligned words.
   Each int16_t is left-shifted by 16 so the TAD5112 DAC receives
   the correct full-scale amplitude in 32-bit mode. */
static void pcm16_to_pcm32(int32_t *dst, const int16_t *src, ULONG count)
{
    for (ULONG i = 0; i < count; i++)
        dst[i] = (int32_t)src[i] << 16;
}

/* Fill one DMA half (half_cnt int32_t words) from the MP3 stream.
   Decoded PCM (int16_t from minimp3) is expanded to int32_t in-place.
   pcm_smp / pcm_off track the current decoded frame in SAMPLES (not bytes).
   Zero-pads if EOF. Returns TX_TRUE if any real audio was written. */
static UINT sai_fill_half_mp3(int32_t *dst, ULONG half_cnt, FX_FILE *file,
    ULONG *mp3_size, UINT *mp3_eof, int *pcm_smp, int *pcm_off,
    UINT *sampling_set)
{
    mp3dec_frame_info_t info;
    ULONG written = 0;   /* samples written to dst so far */

    while (written < half_cnt)
    {
        if (*pcm_smp == 0)
        {
            if (!(*mp3_eof) && *mp3_size < 1440U)
            {
                ULONG to_read = MP3_READ_BUF_SIZE - *mp3_size;
                ULONG nr = 0;
                sai_drain_read(file, s_sai_mp3_read_buf + *mp3_size, to_read, &nr);
                *mp3_size += nr;
                if (nr < to_read) *mp3_eof = 1;
            }
            if (*mp3_size == 0) break;

            int samples  = mp3dec_decode_frame(&s_sai_mp3_decoder,
                                               (const uint8_t *)s_sai_mp3_read_buf,
                                               (int)*mp3_size, s_sai_mp3_pcm_buf, &info);
            int consumed = info.frame_bytes;

            if (samples <= 0)
            {
                if (*mp3_size == MP3_READ_BUF_SIZE)
                {
                    memmove(s_sai_mp3_read_buf, s_sai_mp3_read_buf + 1, *mp3_size - 1);
                    (*mp3_size)--;
                }
                else { break; }
                continue;
            }

            memmove(s_sai_mp3_read_buf, s_sai_mp3_read_buf + consumed, *mp3_size - (ULONG)consumed);
            *mp3_size -= (ULONG)consumed;
            *pcm_smp   = samples * info.channels;  /* total samples in frame */
            *pcm_off   = 0;

            if (!(*sampling_set))
            {
                (void)sai_reconfigure((ULONG)info.hz);
                *sampling_set = 1;
            }
        }

        ULONG to_copy = (ULONG)(*pcm_smp - *pcm_off);
        if (to_copy > half_cnt - written)
            to_copy = half_cnt - written;

        /* Expand 16-bit minimp3 output → 32-bit MSB-aligned for TAD5112 */
        pcm16_to_pcm32(dst + written, s_sai_mp3_pcm_buf + *pcm_off, to_copy);

        written  += to_copy;
        *pcm_off += (int)to_copy;
        if (*pcm_off >= *pcm_smp) *pcm_smp = 0;
    }

    if (written < half_cnt)
        memset(dst + written, 0, (half_cnt - written) * sizeof(int32_t));

    return (written > 0U) ? TX_TRUE : TX_FALSE;
}

/* Diagnostic capture: halts in Error_Handler so the debugger can inspect
   g_sai_tx_result, hsai_BlockA1.ErrorCode, hdma_sai1_a.State/ErrorCode,
   and hdma_sai1_a.LinkedListQueue to identify the exact failure mode. */
static volatile HAL_StatusTypeDef g_sai_tx_result;

static HAL_StatusTypeDef sai_transmit_dma_checked(void)
{
    g_sai_tx_result = HAL_SAI_Transmit_DMA(&hsai_BlockA1,
                                            (uint8_t *)sai_dma_buf,
                                            SAI_HALF_SAMPLES * 4U);
    if (g_sai_tx_result != HAL_OK)
        Error_Handler();
    return g_sai_tx_result;
}

static UINT audio_stream_wav_sai(FX_FILE *file, WAV_INFO *info)
{
    /* Each DMA half: SAI_HALF_SAMPLES stereo pairs = SAI_HALF_SAMPLES*2 samples */
    ULONG half_cnt      = SAI_HALF_SAMPLES * 2U;
    ULONG half_pcm_bytes = half_cnt * sizeof(int16_t);  /* bytes of 16-bit PCM per half */
    ULONG to_read, bytes_read, remaining;
    UINT  zero_fills = 0U;
    UINT  status     = UX_SUCCESS;

    if (sai_reconfigure(info->sample_rate) != HAL_OK)
        return UX_ERROR;

    sai_drain_reset();
    sai_sem_flush();

    /* Mute the amp for exactly 5 ms around SAI start.  The amp is kept powered
       between songs (MUTE_Pin stays HIGH) so its internal bias caps remain fully
       charged; 5 ms of muting means minimal cap discharge and a near-silent
       re-enable.  After the amp is back on, 20 ms of vol=0 silence lets the
       TAD5112 fully settle on the stable BCLK before unmute. */
    tad5112_mute();
    memset(sai_dma_buf, 0, (SAI_HALF_SAMPLES * 4U) * sizeof(int32_t));
    remaining = info->data_size;

    /* MUTE_Pin LOW before BCLK starts: blocks the TAD5112 analog output-stage
       settling transient while the SAI clock restarts.  After 500 ms the DAC
       output is stable at 0 V (vol=0, zeros playing) so re-enabling the amp
       is silent. */
    HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_RESET);
    if (sai_transmit_dma_checked() != HAL_OK)
    {
        HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_SET);
        return UX_ERROR;
    }
    tx_thread_sleep(50);   /* 500 ms: BCLK stable, TAD5112 synced, vol=0 → 0 V out */
    HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_SET);
    tad5112_unmute();
    /* Flush semaphore counts that piled up during the 500 ms sleep.  DMA ran
       ~10 half/full callbacks while we slept; consuming stale counts would make
       the loop write the same DMA half repeatedly without waiting, overlapping
       a write with an active DMA read and producing noise at song start. */
    sai_sem_flush();
    /* Configure USB alt-interface immediately before the first packet is sent,
       then pre-fill the ring with silence so the pipeline is never empty while
       the thread reads SD between SAI half-callbacks. */
    sai_usb_configure(info->sample_rate, info->channels);
    sai_usb_prefill();

    /* Refill one half per DMA callback; zero-fill after EOF then stop */
    while (remaining > 0U || zero_fills < 2U)
    {
        if (tx_semaphore_get(&sai_sem, TX_TIMER_TICKS_PER_SECOND * 2) != TX_SUCCESS)
        { status = UX_ERROR; break; }

        int32_t *half = sai_fill_upper ? (sai_dma_buf + half_cnt) : sai_dma_buf;

        if (remaining > 0U)
        {
            to_read = (remaining > half_pcm_bytes) ? half_pcm_bytes : remaining;
            sai_drain_read(file, (UCHAR *)s_sai_pcm16_tmp, to_read, &bytes_read);
            if (bytes_read < half_pcm_bytes)
                memset((UCHAR *)s_sai_pcm16_tmp + bytes_read, 0, half_pcm_bytes - bytes_read);
            pcm16_to_pcm32(half, s_sai_pcm16_tmp, half_cnt);
            sai_submit_pcm16_to_usb(s_sai_pcm16_tmp, to_read);
            remaining -= to_read;
        }
        else
        {
            memset(half, 0, half_cnt * sizeof(int32_t));
            zero_fills++;
        }
    }

    tad5112_mute();
    /* Brief MUTE_Pin LOW only around DMAStop: blocks the BCLK-stop glitch from
       the TAD5112 output stage, but the amp is off for < 1 ms so its charge-pump
       caps stay charged and re-enable is silent. */
    HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_RESET);
    HAL_SAI_DMAStop(&hsai_BlockA1);
    HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_SET);
    sai_usb_drain();
    sai_sem_flush();
    return status;
}

static UINT audio_stream_mp3_sai(FX_FILE *file)
{
    ULONG half_cnt     = SAI_HALF_SAMPLES * 2U;
    UINT  status       = UX_SUCCESS;
    UINT  sampling_set = 0U, mp3_eof = 0U;
    ULONG mp3_size     = 0U;
    int   pcm_smp      = 0,  pcm_off = 0;  /* in samples, not bytes */
    UINT  zeros        = 0U;
    UINT  had_data     = 0U;
    ULONG probe_hz     = 48000U;
    UINT  probe_ch     = 2U;

    mp3dec_init(&s_sai_mp3_decoder);
    sai_drain_reset();
    sai_sem_flush();

    /* Pre-probe the MP3 sample rate so sai_reconfigure() runs BEFORE DMA starts.
     * Calling it inside sai_fill_half_mp3 on the first DMA callback (while DMA
     * is already running) re-inits the SAI mid-transfer, stops callbacks, and
     * causes the 2-second semaphore wait to time out on every WAV<->MP3 switch. */
    {
        ULONG nr = 0;
        sai_drain_read(file, s_sai_mp3_read_buf, MP3_READ_BUF_SIZE, &nr);
        ULONG probe_size = nr;

        while (probe_size > 0)
        {
            mp3dec_frame_info_t probe_info;
            int s = mp3dec_decode_frame(&s_sai_mp3_decoder,
                        (const uint8_t *)s_sai_mp3_read_buf, (int)probe_size,
                        s_sai_mp3_pcm_buf, &probe_info);
            if (s > 0)
            {
                (void)sai_reconfigure((ULONG)probe_info.hz);
                probe_hz     = (ULONG)probe_info.hz;
                probe_ch     = (UINT)probe_info.channels;
                sampling_set = 1;
                break;
            }
            if (probe_size == (ULONG)MP3_READ_BUF_SIZE)
            {
                memmove(s_sai_mp3_read_buf, s_sai_mp3_read_buf + 1, probe_size - 1);
                probe_size--;
            }
            else
                break;
        }

        /* Reset file and decoder so the streaming pass starts from the beginning. */
        fx_file_seek(file, 0);
        mp3dec_init(&s_sai_mp3_decoder);
        sai_drain_reset();
        mp3_size = 0;
        mp3_eof  = 0;
        pcm_smp  = 0;
        pcm_off  = 0;
    }

    tad5112_mute();
    memset(sai_dma_buf, 0, (SAI_HALF_SAMPLES * 4U) * sizeof(int32_t));

    HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_RESET);
    if (sai_transmit_dma_checked() != HAL_OK)
    {
        HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_SET);
        return UX_ERROR;
    }
    tx_thread_sleep(50);   /* 500 ms: BCLK stable, TAD5112 synced, vol=0 → 0 V out */
    HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_SET);
    tad5112_unmute();
    sai_sem_flush();  /* discard stale counts from DMA callbacks during sleep */
    sai_usb_configure(probe_hz, probe_ch);
    sai_usb_prefill();

    while (zeros < 2U)
    {
        if (tx_semaphore_get(&sai_sem, TX_TIMER_TICKS_PER_SECOND * 2) != TX_SUCCESS)
        { status = UX_ERROR; break; }

        int32_t *half = sai_fill_upper ? (sai_dma_buf + half_cnt) : sai_dma_buf;
        had_data = sai_fill_half_mp3(half, half_cnt, file,
                                      &mp3_size, &mp3_eof, &pcm_smp, &pcm_off, &sampling_set);
        if (s_usb_audio != UX_NULL && s_usb_pkt_size > 0)
        {
            for (ULONG i = 0; i < half_cnt; i++)
                s_usb_dual_pcm16[i] = (int16_t)(half[i] >> 16);
            sai_submit_pcm16_to_usb(s_usb_dual_pcm16, half_cnt * sizeof(int16_t));
        }
        if (!had_data) zeros++;
    }

    tad5112_mute();
    HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_RESET);
    HAL_SAI_DMAStop(&hsai_BlockA1);
    HAL_GPIO_WritePin(MUTE_GPIO_Port, MUTE_Pin, GPIO_PIN_SET);
    sai_usb_drain();
    sai_sem_flush();
    return status;
}

#endif /* AUDIO_OUTPUT_SAI */

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

/*
 * Ring buffer shared by WAV and MP3 streamers.
 *
 * AUDIO_RING_SLOTS  — total packet-sized slots.
 * AUDIO_INFLIGHT    — slots kept submitted to USB at all times.
 *
 * Why this beats the old 2-buffer ping-pong:
 *   Old: submit → wait (sleep 1 tick) → fill → submit  (thread must beat 1 ms deadline)
 *   New: pre-submit AUDIO_INFLIGHT packets; on each completion (blocking wake-up) fill
 *        one new slot and re-submit it immediately.  The AUDIO_INFLIGHT-1 packets still
 *        playing give the thread several milliseconds of headroom, eliminating frame drops
 *        from OS scheduling jitter and SD cluster-boundary reads.
 *
 * Safety: isochronous completions are FIFO, so slot[wr] is always free when the thread
 * reaches it (RING_SLOTS > INFLIGHT guarantees no wrap-around overlap).
 */
/*
 * Ring sizing for MP3:
 *   minimp3 no-SIMD on Cortex-M33 takes ~15-20 ms to decode one 26 ms frame.
 *   AUDIO_INFLIGHT must exceed that worst-case decode gap so the USB pipeline
 *   never runs dry between frames.  At 44.1 kHz stereo 16-bit the packet size
 *   is ~176 bytes (= 1 ms of audio), so AUDIO_INFLIGHT=48 gives ~48 ms headroom.
 *   In dual SAI+USB mode the SAI half fires every ~21 ms; SD reads and ThreadX
 *   scheduling can delay the thread by up to 10 ms, so the pre-filled lead must
 *   exceed (half_period + max_delay) = ~32 ms — AUDIO_INFLIGHT=48 gives 16 ms margin.
 *   Through a hub, the hub interrupt endpoint competes with audio isochronous in
 *   the same USB frame every ~256 ms, adding ~1 ms jitter.  AUDIO_INFLIGHT=60
 *   gives 60 ms headroom, absorbing hub-induced scheduling variance.
 *   AUDIO_RING_SLOTS must be > AUDIO_INFLIGHT to avoid wrap-around overlap.
 *   RAM cost: 64 × 256 = 16 KB.
 */
#define AUDIO_RING_SLOTS  64
#define AUDIO_INFLIGHT    60

static UCHAR audio_ring_buf[AUDIO_RING_SLOTS][AUDIO_MAX_PACKET_SIZE] __attribute__((aligned(4)));
static UX_HOST_CLASS_AUDIO_TRANSFER_REQUEST audio_ring_xfer[AUDIO_RING_SLOTS];

#ifdef AUDIO_OUTPUT_SAI
/* USB completion callback — signals the feeder thread; never blocks. */
static VOID usb_feeder_complete(UX_HOST_CLASS_AUDIO_TRANSFER_REQUEST *xfer)
{
    (void)xfer;
    tx_semaphore_put(&s_usb_xfer_sem);
}

/* Submit ring slot idx with pkt_bytes bytes to the USB speaker. */
static VOID ring_submit_usb(UINT idx, ULONG pkt_bytes)
{
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_data_pointer        = audio_ring_buf[idx];
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_requested_length    = pkt_bytes;
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_packet_size         = s_usb_pkt_size;
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_completion_function = usb_feeder_complete;
    ux_host_class_audio_write(s_usb_audio, &audio_ring_xfer[idx]);
}

/* Dedicated USB feeder thread — runs completely independent of the SAI thread.
   Drains the PCM FIFO and submits USB packets exactly like the USB-only path:
   block on completion → fill next slot from FIFO → submit.
   The fractional accumulator sends the right number of bytes per ms so the
   long-run average equals the SAI sample rate (e.g. 44100 Hz → 9×176 + 1×180
   bytes per 10 ms), preventing FIFO drift that causes cracking. */
static VOID usb_feeder_thread_entry(ULONG arg)
{
    (void)arg;
    UINT  wr       = 0;
    UINT  inflight = 0;

    if (s_usb_pkt_size == 0)
        goto feeder_exit;

    /* Pre-fill AUDIO_INFLIGHT silent packets to prime the pipeline */
    for (UINT i = 0; i < AUDIO_INFLIGHT; i++)
    {
        memset(audio_ring_buf[wr], 0, s_usb_pkt_size);
        ring_submit_usb(wr, s_usb_pkt_size);
        wr = (wr + 1U) % AUDIO_RING_SLOTS;
        inflight++;
    }

    while (1)
    {
        /* Block until one transfer completes (instant wake from callback) */
        if (tx_semaphore_get(&s_usb_xfer_sem, TX_TIMER_TICKS_PER_SECOND) != TX_SUCCESS)
            break;
        inflight--;

        /* Exit when stream stopped and FIFO is drained */
        ULONG avail = (s_usb_fifo_wr - s_usb_fifo_rd) & (USB_PCM_FIFO_SIZE - 1U);
        if (!s_usb_running && avail == 0)
            break;

        /* Fractional accumulator: match exact sample rate so FIFO neither
           overflows nor starves (44100 Hz → alternates 176 / 180 bytes) */
        s_usb_frac_acc    += s_usb_sample_rate;
        ULONG samples      = s_usb_frac_acc / 1000U;
        s_usb_frac_acc    %= 1000U;
        ULONG pkt_bytes    = samples * s_usb_channels_num * sizeof(int16_t);
        if (pkt_bytes == 0 || pkt_bytes > s_usb_pkt_size)
            pkt_bytes = s_usb_pkt_size;

        usb_fifo_read_padded(audio_ring_buf[wr], pkt_bytes);
        ring_submit_usb(wr, pkt_bytes);
        wr = (wr + 1U) % AUDIO_RING_SLOTS;
        inflight++;
    }

    /* Drain any remaining in-flight transfers */
    while (inflight > 0)
    {
        if (tx_semaphore_get(&s_usb_xfer_sem, TX_TIMER_TICKS_PER_SECOND) != TX_SUCCESS)
            break;
        inflight--;
    }

feeder_exit:
    tx_semaphore_put(&s_usb_done_sem);
}

/* Write n_bytes of PCM16 to the FIFO — non-blocking, never stalls the SAI thread. */
static VOID sai_submit_pcm16_to_usb(const int16_t *pcm16, ULONG n_bytes)
{
    if (s_usb_audio == UX_NULL || s_usb_pkt_size == 0)
        return;
    usb_fifo_write((const UCHAR *)pcm16, n_bytes);
}

/* Start the USB feeder thread.  Called just before the SAI main loop. */
static VOID sai_usb_prefill(VOID)
{
    if (s_usb_audio == UX_NULL || s_usb_pkt_size == 0)
        return;
    usb_fifo_reset();
    s_usb_running      = 1U;
    s_usb_frac_acc     = 0;
    s_usb_thread_active = 0;
    tx_semaphore_create(&s_usb_xfer_sem, "usb_xfer", 0);
    tx_semaphore_create(&s_usb_done_sem, "usb_done", 0);
    if (tx_thread_create(&s_usb_thread, "usb_feeder", usb_feeder_thread_entry,
                         0, s_usb_stack, USB_FEEDER_STACK_SIZE,
                         9, 9, TX_NO_TIME_SLICE, TX_AUTO_START) == TX_SUCCESS)
        s_usb_thread_active = 1U;
}

/* Signal stop, wait for feeder thread to finish, then clean up. */
static VOID sai_usb_drain(VOID)
{
    if (!s_usb_thread_active)
        return;
    s_usb_running = 0U;
    (void)tx_semaphore_get(&s_usb_done_sem, 3 * TX_TIMER_TICKS_PER_SECOND);
    tx_thread_terminate(&s_usb_thread);
    tx_thread_delete(&s_usb_thread);
    tx_semaphore_delete(&s_usb_xfer_sem);
    tx_semaphore_delete(&s_usb_done_sem);
    s_usb_thread_active = 0U;
}

/* Select USB alternate interface for the given format. */
static VOID sai_usb_configure(ULONG sample_rate, UINT channels)
{
    if (s_usb_audio == UX_NULL)
        return;
    UX_HOST_CLASS_AUDIO_SAMPLING usb_smp;
    usb_smp.ux_host_class_audio_sampling_channels   = channels;
    usb_smp.ux_host_class_audio_sampling_frequency  = sample_rate;
    usb_smp.ux_host_class_audio_sampling_resolution = 16;
    if (ux_host_class_audio_streaming_sampling_set(s_usb_audio, &usb_smp) == UX_SUCCESS)
        s_usb_pkt_size = ux_host_class_audio_packet_size_get(s_usb_audio);
    else
        s_usb_pkt_size = 0;
    s_usb_sample_rate  = sample_rate;
    s_usb_channels_num = channels;
}
#endif /* AUDIO_OUTPUT_SAI */

/* Drain any stale semaphore counts left by an aborted stream. */
static VOID ring_sem_flush(VOID)
{
    while (tx_semaphore_get(&audio_xfer_semaphore, TX_NO_WAIT) == TX_SUCCESS) {}
}

static UINT ring_submit(UX_HOST_CLASS_AUDIO *audio, UINT idx, ULONG pkt_size)
{
    /* Must be zero: the HCD submits the next transfer as
       data_ptr + actual_length, so a stale value from the previous
       completion causes it to read from the wrong PMA offset → cracking. */
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_actual_length       = 0;
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_data_pointer        = audio_ring_buf[idx];
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_requested_length    = pkt_size;
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_packet_size         = pkt_size;
    audio_ring_xfer[idx].ux_host_class_audio_transfer_request_completion_function = audio_transfer_complete;
    return ux_host_class_audio_write(audio, &audio_ring_xfer[idx]);
}

/* Wait for inflight transfers to complete (abort-safe). */
static VOID ring_drain_inflight(UINT inflight)
{
    while (inflight > 0)
    {
        (void)audio_wait_xfer_done();
        inflight--;
    }
}

static UINT audio_stream_wav(UX_HOST_CLASS_AUDIO *audio, FX_FILE *file, WAV_INFO *info)
{
    UINT  status          = UX_SUCCESS;
    ULONG packet_size;
    ULONG remaining;
    ULONG bytes_read;
    ULONG to_read;
    UINT  wr              = 0;
    UINT  inflight        = 0;
    ULONG frac_acc        = 0;   /* sub-sample fractional accumulator (0..999) */
    ULONG bytes_per_frame;       /* varies per USB frame to hit exact sample rate */
    ULONG bytes_per_sample;
    UX_HOST_CLASS_AUDIO_SAMPLING audio_sampling;

    audio_sampling.ux_host_class_audio_sampling_channels   = info->channels;
    audio_sampling.ux_host_class_audio_sampling_frequency  = info->sample_rate;
    audio_sampling.ux_host_class_audio_sampling_resolution = info->bits_per_sample;

    status = ux_host_class_audio_streaming_sampling_set(audio, &audio_sampling);
    if (status != UX_SUCCESS)
        return status;

    packet_size = ux_host_class_audio_packet_size_get(audio);
    if (packet_size == 0 || packet_size > AUDIO_MAX_PACKET_SIZE)
        return UX_ERROR;

    /* bytes_per_sample used by the fractional accumulator.
     * packet_size = floor(sample_rate/1000) * bytes_per_sample (integer division).
     * mps = endpoint wMaxPacketSize = ceil(sample_rate/1000) * bytes_per_sample
     *       for a compliant device (e.g. 180 for 44100 Hz stereo 16-bit).
     * The accumulator adds the fractional remainder each frame so the long-run
     * average matches the true sample rate: e.g. 44100 Hz → 9×176 + 1×180 per
     * 10 ms window.  bytes_per_frame is capped at mps so the frame always fits
     * in one USB transaction (no spurious 4-byte continuation packets). */
    bytes_per_sample = (ULONG)info->channels * ((ULONG)info->bits_per_sample / 8U);
    {
        ULONG mps = ux_host_class_audio_max_packet_size_get(audio);
        if (mps == 0 || mps > AUDIO_MAX_PACKET_SIZE) mps = packet_size;
        /* Store mps in packet_size; original floor value no longer needed. */
        packet_size = mps;
    }

    drain_reset();
    ring_sem_flush();
    remaining = info->data_size;

#define NEXT_FRAME_BYTES()                                              \
    do {                                                                \
        frac_acc += info->sample_rate % 1000U;                         \
        ULONG _extra = frac_acc / 1000U;                               \
        frac_acc %= 1000U;                                             \
        bytes_per_frame = (info->sample_rate / 1000U + _extra)         \
                          * bytes_per_sample;                           \
        if (bytes_per_frame > packet_size)                             \
            bytes_per_frame = packet_size;                             \
    } while (0)

    /* ---- Phase 1: pre-fill AUDIO_INFLIGHT slots before entering steady state ---- */
    while (inflight < AUDIO_INFLIGHT && remaining > 0)
    {
        NEXT_FRAME_BYTES();
        to_read = (remaining > bytes_per_frame) ? bytes_per_frame : remaining;
        status  = drain_read(file, audio_ring_buf[wr], to_read, &bytes_read);
        if (status != FX_SUCCESS || bytes_read == 0)
            goto done;

        if (bytes_read < bytes_per_frame)
            memset(audio_ring_buf[wr] + bytes_read, 0, bytes_per_frame - bytes_read);

        status = ring_submit(audio, wr, bytes_per_frame);
        if (status != UX_SUCCESS)
            goto done;

        wr = (wr + 1U) % AUDIO_RING_SLOTS;
        inflight++;
        remaining -= to_read;
    }

    /* ---- Phase 2: steady state — one completion in, one new packet out ---- */
    while (remaining > 0)
    {
        status = audio_wait_xfer_done();
        if (status != UX_SUCCESS)
            goto done;
        inflight--;

        NEXT_FRAME_BYTES();
        to_read = (remaining > bytes_per_frame) ? bytes_per_frame : remaining;
        status  = drain_read(file, audio_ring_buf[wr], to_read, &bytes_read);
        if (status != FX_SUCCESS || bytes_read == 0)
            goto done;

        if (bytes_read < bytes_per_frame)
            memset(audio_ring_buf[wr] + bytes_read, 0, bytes_per_frame - bytes_read);

        status = ring_submit(audio, wr, bytes_per_frame);
        if (status != UX_SUCCESS)
            goto done;

        wr = (wr + 1U) % AUDIO_RING_SLOTS;
        inflight++;
        remaining -= to_read;
    }

#undef NEXT_FRAME_BYTES

done:
    ring_drain_inflight(inflight);
    return (status == UX_SUCCESS || status == FX_SUCCESS) ? UX_SUCCESS : status;
}

static UINT str_ends_with_wav(CHAR *name)
{
    UINT len = 0;
    while (name[len] != '\0')
        len++;
    if (len < 4)
        return 0;
    return ((name[len-4] == '.') &&
            (name[len-3] == 'w' || name[len-3] == 'W') &&
            (name[len-2] == 'a' || name[len-2] == 'A') &&
            (name[len-1] == 'v' || name[len-1] == 'V'));
}

static UINT str_ends_with_mp3(CHAR *name)
{
    UINT len = 0;
    while (name[len] != '\0')
        len++;
    if (len < 4)
        return 0;
    return ((name[len-4] == '.') &&
            (name[len-3] == 'm' || name[len-3] == 'M') &&
            (name[len-2] == 'p' || name[len-2] == 'P') &&
            (name[len-1] == '3'));
}

static UINT audio_stream_mp3(UX_HOST_CLASS_AUDIO *audio, FX_FILE *file)
{
    mp3dec_frame_info_t info;
    ULONG mp3_size       = 0;
    UINT  sampling_set   = 0;
    UX_HOST_CLASS_AUDIO_SAMPLING audio_sampling;
    ULONG packet_size    = AUDIO_MAX_PACKET_SIZE;
    ULONG next_slot_size = AUDIO_MAX_PACKET_SIZE;  /* per-packet target (fractional rate) */
    ULONG frac_acc       = 0;
    ULONG bytes_per_sample = 0;
    UINT  wr             = 0;
    UINT  inflight       = 0;
    UINT  status         = UX_SUCCESS;
    int   consumed, samples, pcm_bytes;
    ULONG slot_fill      = 0;

    mp3dec_init(&s_mp3_decoder);
    drain_reset();
    ring_sem_flush();

    UINT mp3_eof = 0;

    while (1)
    {
        /* Refill the MP3 input window whenever it drops below one max frame.
           drain_read buffers 4 KB chunks from SD so the refill rarely stalls
           on a raw fx_file_read — same benefit the WAV path gets. */
        if (!mp3_eof && mp3_size < 1440)
        {
            ULONG to_read = MP3_READ_BUF_SIZE - mp3_size;
            ULONG nr = 0;
            drain_read(file, s_mp3_read_buf + mp3_size, to_read, &nr);
            mp3_size += nr;
            if (nr < to_read)
                mp3_eof = 1;  /* short read → EOF, drain remaining buffer */
            if (mp3_size == 0)
                break;
        }

        samples  = mp3dec_decode_frame(&s_mp3_decoder, (const uint8_t *)s_mp3_read_buf,
                                       mp3_size, s_mp3_pcm_buf, &info);
        consumed = info.frame_bytes;

        if (samples <= 0)
        {
            if (mp3_size == 0) break;
            if (mp3_size == MP3_READ_BUF_SIZE)
            {
                memmove(s_mp3_read_buf, s_mp3_read_buf + 1, mp3_size - 1);
                mp3_size--;
            }
            else break;
            continue;
        }

        memmove(s_mp3_read_buf, s_mp3_read_buf + consumed, mp3_size - consumed);
        mp3_size -= (ULONG)consumed;

        /* samples is per-channel; multiply by channels for total PCM bytes. */
        pcm_bytes = samples * info.channels * (int)sizeof(mp3d_sample_t);

        if (!sampling_set)
        {
            audio_sampling.ux_host_class_audio_sampling_channels   = (ULONG)info.channels;
            audio_sampling.ux_host_class_audio_sampling_frequency  = (ULONG)info.hz;
            audio_sampling.ux_host_class_audio_sampling_resolution = 16;
            status = ux_host_class_audio_streaming_sampling_set(audio, &audio_sampling);
            if (status != UX_SUCCESS)
                goto mp3_done;
            packet_size = ux_host_class_audio_packet_size_get(audio);
            if (packet_size == 0 || packet_size > AUDIO_MAX_PACKET_SIZE)
            {
                status = UX_ERROR;
                goto mp3_done;
            }
            {
                ULONG mps = ux_host_class_audio_max_packet_size_get(audio);
                if (mps == 0 || mps > AUDIO_MAX_PACKET_SIZE) mps = packet_size;
                packet_size = mps;
            }
            bytes_per_sample = (ULONG)info.channels * 2U;  /* 16-bit */
            next_slot_size   = packet_size;
            frac_acc         = 0;
            sampling_set     = 1;
        }

        /* Pack PCM into ring slots without breaking at frame boundaries.
         * slot_fill persists across frames: the tail bytes of one frame
         * fill the start of the next slot instead of being zero-padded.
         * Zero-padding only happens once, on the very last partial slot at EOF.
         * Without this, 44.1 kHz MP3 (4608 PCM bytes, 176-byte packets) inserts
         * 144 silence bytes every 26 ms (~38 Hz buzz).
         *
         * next_slot_size varies per packet using a fractional accumulator so the
         * long-run average matches the true sample rate (e.g. 44100 Hz →
         * 9×176 + 1×180 bytes per 10-ms window). */
        int pcm_offset = 0;
        while (pcm_offset < pcm_bytes)
        {
            int space = (int)next_slot_size - (int)slot_fill;
            int take  = pcm_bytes - pcm_offset;
            if (take > space) take = space;

            memcpy(audio_ring_buf[wr] + slot_fill,
                   (UCHAR *)s_mp3_pcm_buf + pcm_offset, (ULONG)take);
            slot_fill  += (ULONG)take;
            pcm_offset += take;

            if (slot_fill >= next_slot_size)
            {
                if (inflight == AUDIO_INFLIGHT)
                {
                    status = audio_wait_xfer_done();
                    if (status != UX_SUCCESS)
                        goto mp3_done;
                    inflight--;
                }
                status = ring_submit(audio, wr, next_slot_size);
                if (status != UX_SUCCESS)
                    goto mp3_done;
                wr = (wr + 1U) % AUDIO_RING_SLOTS;
                inflight++;
                slot_fill = 0;

                /* Compute size for next slot using fractional accumulator */
                if (bytes_per_sample > 0)
                {
                    frac_acc += (ULONG)info.hz % 1000U;
                    ULONG _extra = frac_acc / 1000U;
                    frac_acc %= 1000U;
                    next_slot_size = ((ULONG)info.hz / 1000U + _extra) * bytes_per_sample;
                    if (next_slot_size > packet_size)   /* cap at MPS */
                        next_slot_size = packet_size;
                }
            }
        }
    }

    /* Flush partial last slot at EOF */
    if (slot_fill > 0 && next_slot_size > 0)
    {
        memset(audio_ring_buf[wr] + slot_fill, 0, next_slot_size - slot_fill);
        if (inflight == AUDIO_INFLIGHT)
        {
            (void)audio_wait_xfer_done();
            inflight--;
        }
        (void)ring_submit(audio, wr, next_slot_size);
        inflight++;
    }

mp3_done:
    ring_drain_inflight(inflight);
    return (status == UX_SUCCESS || status == FX_SUCCESS) ? UX_SUCCESS : status;
}

/* USER CODE END 0 */

/* USER CODE BEGIN 1 */

VOID audio_playback_wav_files(UX_HOST_CLASS_AUDIO *audio, FX_MEDIA *media)
{
    UINT status;
    CHAR entry_name[FX_MAX_LONG_NAME_LEN];
    UINT file_attributes;
    ULONG file_size;
    FX_FILE audio_file;
    WAV_INFO wav_info;
    FX_LOCAL_PATH local_path;

    s_audio_playback_abort = TX_FALSE;
    s_audio_playback_active_usb = TX_TRUE;
    tx_semaphore_create(&audio_xfer_semaphore, "audio_xfer_sem", 0);
    fx_directory_local_path_set(media, &local_path, "/");

    while (1)
    {
        status = fx_directory_first_entry_find(media, entry_name);

        /* If the directory scan itself fails (SD removed, media error) exit
           cleanly. Without this, the outer while(1) spins forever, s_audio_
           playback_active never clears, and the FileX thread deadlocks waiting
           for audio_playback_is_active() to return false. */
        if (status != FX_SUCCESS && status != FX_NO_MORE_ENTRIES)
            goto done;

        while (status == FX_SUCCESS)
        {
            fx_directory_information_get(media, entry_name, &file_attributes,
                                         &file_size, UX_NULL, UX_NULL,
                                         UX_NULL, UX_NULL, UX_NULL, UX_NULL);

            if (!(file_attributes & FX_DIRECTORY) && str_ends_with_wav(entry_name))
            {
                status = fx_file_open(media, &audio_file, entry_name, FX_OPEN_FOR_READ);
                /* File found in directory but cannot open = SD removed or IO error.
                   Must goto done — silently skipping would loop forever on cached
                   directory entries, keeping s_audio_playback_active TRUE and
                   deadlocking the FileX thread. */
                if (status != FX_SUCCESS)
                    goto done;
                status = wav_parse_header(&audio_file, &wav_info);
                if (status == FX_SUCCESS)
                    status = audio_stream_wav(audio, &audio_file, &wav_info);
                fx_file_close(&audio_file);
                if (status != FX_SUCCESS && status != UX_SUCCESS)
                    goto done;
            }
            else if (!(file_attributes & FX_DIRECTORY) && str_ends_with_mp3(entry_name))
            {
                status = fx_file_open(media, &audio_file, entry_name, FX_OPEN_FOR_READ);
                if (status != FX_SUCCESS)
                    goto done;
                status = audio_stream_mp3(audio, &audio_file);
                fx_file_close(&audio_file);
                if (status != FX_SUCCESS && status != UX_SUCCESS)
                    goto done;
            }

            status = fx_directory_next_entry_find(media, entry_name);
        }
    }

done:
    s_audio_playback_active_usb = TX_FALSE;
    /* Do NOT call ux_host_class_audio_stop when:
       - s_audio_playback_abort is set: USBX already freed the instance on disconnect.
       - isochronous_endpoint is NULL: alt setting 0 was never activated (no transfers
         were ever submitted), so abort inside USBX dereferences a NULL endpoint. */
    if (!s_audio_playback_abort &&
        audio->ux_host_class_audio_isochronous_endpoint != UX_NULL)
        (void)ux_host_class_audio_stop(audio);
    s_audio_playback_abort = TX_FALSE;
    tx_semaphore_delete(&audio_xfer_semaphore);
}

#ifdef AUDIO_OUTPUT_SAI
VOID audio_playback_sai_files(FX_MEDIA *media, UX_HOST_CLASS_AUDIO *usb_audio)
{
    UINT  status;
    CHAR  entry_name[FX_MAX_LONG_NAME_LEN];
    UINT  file_attributes;
    ULONG file_size;
    FX_FILE audio_file;
    WAV_INFO wav_info;
    FX_LOCAL_PATH local_path;

    s_usb_audio         = usb_audio;
    s_usb_pkt_size      = 0;
    s_usb_sample_rate   = 0;
    s_usb_channels_num  = 0;
    s_usb_frac_acc      = 0;
    s_usb_running       = 0;
    s_usb_thread_active = 0;
    usb_fifo_reset();

    s_audio_playback_active = TX_TRUE;
    tx_semaphore_create(&sai_sem, "sai_sem", 0);
    fx_directory_local_path_set(media, &local_path, "/");

    while (1)
    {
        status = fx_directory_first_entry_find(media, entry_name);
        if (status != FX_SUCCESS && status != FX_NO_MORE_ENTRIES)
            goto sai_done;

        while (status == FX_SUCCESS)
        {
            fx_directory_information_get(media, entry_name, &file_attributes,
                                         &file_size, UX_NULL, UX_NULL,
                                         UX_NULL, UX_NULL, UX_NULL, UX_NULL);

            if (!(file_attributes & FX_DIRECTORY) && str_ends_with_wav(entry_name))
            {
                status = fx_file_open(media, &audio_file, entry_name, FX_OPEN_FOR_READ);
                if (status != FX_SUCCESS) goto sai_done;
                status = wav_parse_header(&audio_file, &wav_info);
                if (status == FX_SUCCESS)
                    status = audio_stream_wav_sai(&audio_file, &wav_info);
                fx_file_close(&audio_file);
                if (status != FX_SUCCESS && status != UX_SUCCESS) goto sai_done;
            }
            else if (!(file_attributes & FX_DIRECTORY) && str_ends_with_mp3(entry_name))
            {
                status = fx_file_open(media, &audio_file, entry_name, FX_OPEN_FOR_READ);
                if (status != FX_SUCCESS)
                	goto sai_done;
                status = audio_stream_mp3_sai(&audio_file);
                fx_file_close(&audio_file);
                if (status != FX_SUCCESS && status != UX_SUCCESS) goto sai_done;
            }

            status = fx_directory_next_entry_find(media, entry_name);
        }
    }

sai_done:
    HAL_SAI_DMAStop(&hsai_BlockA1);
    sai_usb_drain();
    if (s_usb_audio != UX_NULL &&
        s_usb_audio->ux_host_class_audio_isochronous_endpoint != UX_NULL)
        (void)ux_host_class_audio_stop(s_usb_audio);
    s_usb_audio = UX_NULL;
    s_audio_playback_active = TX_FALSE;
    tx_semaphore_delete(&sai_sem);
}
#endif /* AUDIO_OUTPUT_SAI */

/* USER CODE END 1 */
