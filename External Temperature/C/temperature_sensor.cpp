/*
 * temperature_sensor.cpp — DB thermistor driver
 *
 * All four channels use a single ADC1 peripheral with dynamic channel
 * switching. temp_sensor_init() overrides the CubeMX 4-channel scan
 * config to single-conversion mode; read_channel() then reconfigures
 * the channel register before each measurement.
 */

#include "temperature_sensor.h"
#include "main.h"
#include <math.h>
#include <string.h>

/* ── Steinhart–Hart beta equation constants (PS103J2 thermistor) ── */
#define REFERENCE_TEMP_K        298.15f    /* 25 °C in Kelvin          */
#define REFERENCE_RESISTANCE    10000.0f   /* 10 kΩ nominal at 25 °C   */
#define BETA_COEFFICIENT        3892.0f
#define ADC_MAX_COUNTS          4095.0f
#define ADC_REF_VOLTAGE         3.29f      /* VREF+ rail               */
#define FIXED_DIVIDER_RESISTOR  15000.0f   /* pull-up resistor         */
#define KELVIN_OFFSET           273.15f
#define N_SAMPLES               16         /* averaging window         */
#define ADC_TIMEOUT_MS          50

/* PC0–PC3 → ADC1_IN10–IN13 */
static const uint32_t CHANNELS[4] = {
    ADC_CHANNEL_10,
    ADC_CHANNEL_11,
    ADC_CHANNEL_12,
    ADC_CHANNEL_13,
};

/* ── Beta equation: voltage → °C ── */
static float voltage_to_temp(float v)
{
    if (v < 0.001f) return TEMPERATURE_ERROR_SENTINEL;  /* open-circuit */

    float R   = (ADC_REF_VOLTAGE * FIXED_DIVIDER_RESISTOR) / v
                 - FIXED_DIVIDER_RESISTOR;
    float inv = (1.0f / REFERENCE_TEMP_K)
                + (1.0f / BETA_COEFFICIENT) * logf(R / REFERENCE_RESISTANCE);
    return (1.0f / inv) - KELVIN_OFFSET;
}

/* ── Single-channel read: 1 dummy + N_SAMPLES averaged ── */
static float read_channel(uint8_t ch)
{
    HAL_ADC_Stop(&hadc1);

    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel      = CHANNELS[ch];
    cfg.Rank         = 1;                    /* always rank 1: one channel at a time */
    cfg.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &cfg) != HAL_OK)
        return TEMPERATURE_ERROR_SENTINEL;

    /* Dummy conversion — settles ADC input capacitor after mux switch */
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS);
    (void)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* Averaged measurement */
    uint32_t sum = 0;
    for (int i = 0; i < N_SAMPLES; i++) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS) != HAL_OK) {
            HAL_ADC_Stop(&hadc1);
            return TEMPERATURE_ERROR_SENTINEL;
        }
        sum += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }

    float raw  = (float)sum / (float)N_SAMPLES;
    float volt = (raw / ADC_MAX_COUNTS) * ADC_REF_VOLTAGE;
    return voltage_to_temp(volt);
}

/* ── Public API ── */

void temp_sensor_init(void)
{
    /*
     * CubeMX generates MX_ADC1_Init() with ScanConvMode=ENABLE and
     * NbrOfConversion=4 when four channels are configured with ranks 1–4.
     * read_channel() uses dynamic single-channel switching (rank always 1),
     * which conflicts with a 4-rank scan sequence.
     *
     * Override here to single-conversion mode so each HAL_ADC_Start()
     * converts exactly the one channel configured by the preceding
     * HAL_ADC_ConfigChannel() call.
     */
    HAL_ADC_Stop(&hadc1);
    hadc1.Init.ScanConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.EOCSelection    = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);
}

void temp_sensor_read(uint8_t *out, uint8_t *len_out)
{
    for (uint8_t ch = 0; ch < 4; ch++) {
        float t = read_channel(ch);
        memcpy(out + (ch * sizeof(float)), &t, sizeof(float));
    }
    if (len_out) *len_out = 16;
}
