/*
 * emi.c
 *
 * Lock-in EMI measurement adapted from the EMITesting (STM32H7A3) reference
 * project for the STM32F405 motherboard, using HSE crystal clock source and
 * tim2-triggered DMA for phase-locked DAC output and ADC capture.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * .ioc / CubeIDE setup required before this file will compile and run:
 *
 *  RCC
 *    HSE  →  Crystal/Ceramic Resonator   (uses PH0 / PH1 already assigned)
 *    Clock Configuration tab:
 *      PLL Source   = HSE
 *      PLLM         = (your crystal MHz)   e.g. 8  for an 8 MHz crystal
 *      PLLN         = 336
 *      PLLP         = 2
 *      → SYSCLK = 168 MHz, APB1 = 42 MHz, tim2 clock = 84 MHz
 *
 *  tim2  (Timers → tim2)
 *    Activated               ✓
 *    Prescaler               0
 *    Counter Period          39     ← gives 84 MHz / 40 = 2.1 MHz sample rate
 *    Trigger Event (TRGO)    Update Event
 *    (No interrupt needed — DMA handles completion)
 *
 *  DAC  (Analog → DAC)
 *    OUT1 / PA4
 *      Output Buffer         Enable
 *      Trigger               Timer 6 Trigger Out Event   ← change from None
 *      DMA Settings → Add:
 *        Request             DAC1
 *        Stream              DMA1 Stream 5
 *        Direction           Memory To Peripheral
 *        Priority            High
 *        Mode                Circular
 *        Peripheral DW       Half Word
 *        Memory DW           Half Word
 *        Memory Increment    ✓
 *
 *  ADC2  (Analog → ADC2)
 *    Pin PC3 → select ADC2_IN13 in the pin map
 *    Parameter Settings:
 *      Clock Prescaler              PCLK2 divided by 2
 *      Resolution                   12 bits
 *      Scan Conversion Mode         Disabled
 *      Continuous Conversion Mode   Disabled
 *      DMA Continuous Requests      Enabled          ← required
 *      End Of Conversion            EOC single conv
 *      External Trigger Conversion  Timer 6 Trigger Out Event
 *      External Trigger Edge        Rising Edge
 *    Rank 1:
 *      Channel                      Channel 13
 *      Sampling Time                3 Cycles         ← fast enough for 2.1 MHz
 *    DMA Settings → Add:
 *        Request             ADC2
 *        Stream              DMA2 Stream 2  (or Stream 3)
 *        Direction           Peripheral To Memory
 *        Priority            High
 *        Mode                Normal           ← NOT circular; stops after N samples
 *        Peripheral DW       Half Word
 *        Memory DW           Half Word
 *        Memory Increment    ✓
 *    NVIC Settings:
 *      DMA2 StreamX global interrupt  ✓  (CubeIDE enables this automatically)
 *
 *  PA2 / ADC3_IN2  (shunt resistor signal)
 *    Pin PA2 → select ADC3_IN2 in the pin map
 *    Not used by emi.c yet — reserved for a future drive-current
 *    normalisation channel.  Configure ADC3 if needed later.
 *
 * After saving the .ioc and regenerating code:
 *   • MX_tim2_Init(), MX_ADC2_Init() (updated), MX_DAC_Init() (updated)
 *     and the matching DMA stream init/NVIC code appear automatically.
 *   • extern TIM_HandleTypeDef htim2; is added to main.h by CubeIDE.
 *   • hadc2 and hdac are already declared in main.h.
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * Algorithm (identical to the EMITesting reference):
 *   1. Build a 128-point 12-bit sine LUT for the DAC and float sin/cos
 *      reference LUTs for lock-in demodulation.
 *   2. Start tim2; DAC DMA loops the LUT continuously (circular); ADC2 DMA
 *      captures EMI_TOTAL_SAMPLES samples in normal (one-shot) mode.
 *   3. Wait for ADC DMA complete callback, then stop everything.
 *   4. Discard the first EMI_SETTLE_SAMPLES (hardware settling), compute
 *      I and Q by correlating the remainder with the reference LUTs.
 *   5. Apply the I/Q rotation matrix, then subtract the baseline offsets.
 *   6. Repeat EMI_NUM_REPS times, accumulate, return the averages.
 *
 * Timing (168 MHz SYSCLK, 8 MHz HSE crystal, tim2 Period=39):
 *   tim2 clock    = 84 MHz   (2 × APB1)
 *   Sample rate   = 84 MHz / 40 = 2.1 MHz
 *   Drive freq    = 2.1 MHz / 128 pts ≈ 16.4 kHz
 *   ADC conv time = (3+12) / 42 MHz ≈ 357 ns  <  476 ns (timer period) ✓
 *   Per rep time  = 38 400 samples / 2.1 MHz ≈ 18.3 ms
 *   Total (50 rep)≈ 914 ms
 */

#include "emi.h"
#include "main.h"   /* hadc2, hdac, htim2 */
#include <math.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════ */
/* Measurement parameters                                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

/** Sine-wave LUT length — matches EMITesting reference (128 points). */
#define EMI_LUT_SIZE            128u

/** 12-bit DAC midpoint and amplitude. */
#define EMI_DAC_MID             2048u
#define EMI_DAC_AMP             850u

/** Settling cycles discarded before lock-in accumulation begins. */
#define EMI_SETTLING_CYCLES     100u

/** Cycles used for I/Q lock-in computation — matches EMITesting reference. */
#define EMI_MEAS_CYCLES         200u

/** Number of repetitions to average. */
#define EMI_NUM_REPS            50u

/**
 * Lock-in reference parameters — matches EMITesting reference.
 * REF_STEP=1 demodulates at the fundamental drive frequency (16.4 kHz).
 * LOCKIN_LUT_SHIFT = LUT_SIZE/2 - 1 = 63, matching the reference project.
 */
#define EMI_REF_STEP            2u
#define EMI_LOCKIN_LUT_SHIFT    61u //65

/** I/Q rotation angle in degrees. Re-characterise for your coil geometry. */
#define EMI_IQ_ROTATION_DEG    54.0f //135.5 -Q(8-12) 133.4 -Q(~-9)

/**
 * Baseline I/Q offsets measured with no target present (after rotation).
 *   corr_i = rot_i - BASELINE_I
 *   corr_q = rot_q - BASELINE_Q
 * Re-measure these after any mechanical changes to the coil assembly.
 */
#define EMI_BASELINE_I          206.00f
#define EMI_BASELINE_Q          0.00f

#define EMI_PI                  3.14159265f

/* ═══════════════════════════════════════════════════════════════════════════ */
/* Derived constants                                                            */
/* ═══════════════════════════════════════════════════════════════════════════ */

#define EMI_TOTAL_CYCLES    (EMI_SETTLING_CYCLES + EMI_MEAS_CYCLES)
#define EMI_TOTAL_SAMPLES   (EMI_TOTAL_CYCLES  * EMI_LUT_SIZE)   /* 38 400 */
#define EMI_SETTLE_SAMPLES  (EMI_SETTLING_CYCLES * EMI_LUT_SIZE) /* 12 800 */
#define EMI_MEAS_SAMPLES    (EMI_MEAS_CYCLES   * EMI_LUT_SIZE)   /* 25 600 */

/* ═══════════════════════════════════════════════════════════════════════════ */
/* Static storage                                                               */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Full ADC capture buffer — settling + measurement window.
 * 38 400 × 2 bytes = 76.8 KB.  Sits in BSS (zero-initialised, no flash cost).
 * The STM32F405 has 192 KB SRAM so this fits comfortably.
 */
static uint16_t s_adc_buf[EMI_TOTAL_SAMPLES];

/** DAC 12-bit sine LUT (uint16_t so DAC DMA can transfer half-words). */
static uint16_t s_dac_lut[EMI_LUT_SIZE];

/** Reference LUTs for I/Q demodulation. */
static float s_ref_sin[EMI_LUT_SIZE];
static float s_ref_cos[EMI_LUT_SIZE];

/** Set to 1 by HAL_ADC_ConvCpltCallback when DMA transfer is complete. */
static volatile uint8_t s_adc_done = 0;

/* ═══════════════════════════════════════════════════════════════════════════ */
/* DMA completion callback                                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Called by the HAL when ADC2's DMA Normal-mode transfer completes
 * (i.e. all EMI_TOTAL_SAMPLES have been captured).
 * This overrides the __weak default in the HAL library.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC2)
    {
        s_adc_done = 1;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* Internal: one DMA-triggered capture                                          */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Fills s_adc_buf[] with one complete capture (settling + measurement).
 *
 * Sequence mirrors start_capture() / stop_capture() from EMITesting:
 *   1. Zero the ADC buffer so stale data cannot corrupt a partial capture.
 *   2. Reset tim2 and set DAC to mid-rail.
 *   3. Start ADC2 DMA (Normal mode — stops automatically after TOTAL_SAMPLES).
 *   4. Start DAC DMA (Circular mode — loops LUT continuously).
 *   5. Start tim2 — this is the single switch that fires both DMAs.
 *   6. Spin until s_adc_done (set by ConvCpltCallback), then tear down.
 */
static void capture_one_rep(void)
{
    s_adc_done = 0;

    /* Zero the buffer — matches reference start_capture() behaviour and
     * ensures stale data cannot be processed if DMA stops early. */
    memset(s_adc_buf, 0, sizeof(s_adc_buf));

    /* Park DAC at mid-rail before arming DMA. */
    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, EMI_DAC_MID);

    /* Reset timer counter so phase is deterministic each rep. */
    HAL_TIM_Base_Stop(&htim2);
    __HAL_TIM_SET_COUNTER(&htim2, 0);

    /*
     * Arm ADC2 DMA — Normal mode, EMI_TOTAL_SAMPLES half-words.
     * Each tim2 TRGO rising edge triggers one ADC conversion; the EOC
     * fires a DMA request.  DMAContinuousRequests=ENABLE in the .ioc
     * keeps this working for every trigger, not just the first.
     */
    if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)s_adc_buf,
                          EMI_TOTAL_SAMPLES) != HAL_OK)
    {
        return; /* ADC DMA failed — caller will see zeroed output */
    }

    /*
     * Arm DAC DMA — Circular mode, loops s_dac_lut[] indefinitely.
     * Each tim2 TRGO event advances the DAC by one LUT step, keeping
     * the sine drive synchronised with the ADC samples.
     */
    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1,
                          (uint32_t *)s_dac_lut, EMI_LUT_SIZE,
                          DAC_ALIGN_12B_R) != HAL_OK)
    {
        HAL_ADC_Stop_DMA(&hadc2);
        return;
    }

    /* Fire tim2 — both DMAs are now clocked by its TRGO. */
    HAL_TIM_Base_Start(&htim2);

    /* Wait for ADC DMA Normal transfer complete (set in callback). */
    uint32_t deadline = HAL_GetTick() + 5000UL; /* 5 s safety timeout */
    while (!s_adc_done && (HAL_GetTick() < deadline))
    {
        /* Bare-metal busy-wait.  The DMA interrupt runs in the background. */
    }

    /* Stop timer first to prevent further DMA requests. */
    HAL_TIM_Base_Stop(&htim2);
    HAL_ADC_Stop_DMA(&hadc2);
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);

    /* Return DAC to mid-rail. */
    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, EMI_DAC_MID);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* Internal: process one capture → corrected I, Q, magnitude, phase           */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Processes s_adc_buf[] over the measurement window (skipping settling
 * samples) and returns baseline-corrected I/Q values.
 *
 * Mirrors process_lockin() → rotate_iq() → subtract_baseline() from the
 * EMITesting reference, collapsed into one function.
 */
static void process_one_rep(float *ci_out, float *cq_out,
                             float *mag_out, float *ph_out)
{
    /* ── Step 1: DC bias removal (mean of measurement window only) ── */
    float mean = 0.0f;
    for (uint32_t n = 0; n < EMI_MEAS_SAMPLES; n++)
        mean += (float)s_adc_buf[EMI_SETTLE_SAMPLES + n];
    mean /= (float)EMI_MEAS_SAMPLES;

    /* ── Step 2: Lock-in I/Q correlation ── */
    float i_sum = 0.0f;
    float q_sum = 0.0f;

    for (uint32_t n = 0; n < EMI_MEAS_SAMPLES; n++)
    {
        float sample = (float)s_adc_buf[EMI_SETTLE_SAMPLES + n] - mean;

        uint16_t ph_idx = (uint16_t)(
            ((uint32_t)(n * EMI_REF_STEP) + EMI_LOCKIN_LUT_SHIFT)
            % EMI_LUT_SIZE);

        i_sum += sample * s_ref_sin[ph_idx];
        q_sum += sample * s_ref_cos[ph_idx];
    }

    float li = (2.0f * i_sum) / (float)EMI_MEAS_SAMPLES;
    float lq = (2.0f * q_sum) / (float)EMI_MEAS_SAMPLES;

    /* ── Step 3: I/Q rotation matrix ── */
    float theta = EMI_IQ_ROTATION_DEG * (EMI_PI / 180.0f);
    float rc = cosf(theta);
    float rs = sinf(theta);
    float ri = (li * rc) - (lq * rs);
    float rq = (li * rs) + (lq * rc);

    /* ── Step 4: Baseline subtraction ── */
    float ci = ri - EMI_BASELINE_I;
    float cq = rq - EMI_BASELINE_Q;

    *ci_out  = ci;
    *cq_out  = cq;
    *mag_out = sqrtf((ci * ci) + (cq * cq));
    *ph_out  = atan2f(cq, ci) * (180.0f / EMI_PI);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* Public API                                                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

void emi_init(void)
{
    for (uint16_t i = 0; i < EMI_LUT_SIZE; i++)
    {
        float angle = 2.0f * EMI_PI * ((float)i / (float)EMI_LUT_SIZE);
        float v = (float)EMI_DAC_MID + (float)EMI_DAC_AMP * sinf(angle);
        if (v < 0.0f)    v = 0.0f;
        if (v > 4095.0f) v = 4095.0f;
        s_dac_lut[i] = (uint16_t)(v + 0.5f);
        s_ref_sin[i] = sinf(angle);
        s_ref_cos[i] = cosf(angle);
    }
}

void emi_read(uint8_t *out)
{
    /*
     * Accumulation buffers — mirror print_average_results() variables
     * from the EMITesting reference project.
     */
    float sum_i   = 0.0f;
    float sum_q   = 0.0f;
    float sum_mag = 0.0f;
    float ph_sin  = 0.0f;   /* circular-mean phase accumulators */
    float ph_cos  = 0.0f;

    for (uint8_t r = 0; r < EMI_NUM_REPS; r++)
    {
        float ci, cq, cmag, cph;

        capture_one_rep();
        process_one_rep(&ci, &cq, &cmag, &cph);

        sum_i   += ci;
        sum_q   += cq;
        sum_mag += cmag;

        float ph_rad = cph * (EMI_PI / 180.0f);
        ph_sin += sinf(ph_rad);
        ph_cos += cosf(ph_rad);
    }

    float avg_i       = sum_i   / (float)EMI_NUM_REPS;
    float avg_q       = sum_q   / (float)EMI_NUM_REPS;
    float avg_mag     = sum_mag / (float)EMI_NUM_REPS;
    float avg_ph_circ = atan2f(ph_sin, ph_cos) * (180.0f / EMI_PI); /* avg_corr_phase_deg     */
    float avg_ph_iq   = atan2f(avg_q,  avg_i)  * (180.0f / EMI_PI); /* avg_corr_phase_from_iq */

    /*
     * Pack in printf order from print_average_results():
     *   avg_corr_i, avg_corr_q, avg_corr_mag,
     *   avg_corr_phase_deg, avg_corr_phase_from_iq
     */
    memcpy(out +  0, &avg_i,       sizeof(float));
    memcpy(out +  4, &avg_q,       sizeof(float));
    memcpy(out +  8, &avg_mag,     sizeof(float));
    memcpy(out + 12, &avg_ph_circ, sizeof(float));
    memcpy(out + 16, &avg_ph_iq,   sizeof(float));
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
    HAL_Delay(500);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
}
