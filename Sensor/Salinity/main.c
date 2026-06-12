/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — Conductivity / Salinity Measurement
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
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include <math.h>
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum {
    COND_OK = 0,
    COND_FAULT_SHORT,       /* ADC read ~3.3 V before excitation — short/fault */
    COND_FAULT_TIMEOUT      /* DMA never completed                              */
} ConductivityStatus;

typedef struct {
    ConductivityStatus  status;
    float               adc1_rms_V;       /* Current channel RMS voltage (V)  */
    float               adc2_rms_V;       /* Voltage channel RMS voltage (V)  */
    float               conductivity_mS;
    float               salinity_gL;
} ConductivityResult;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* --- Sine / DAC ---------------------------------------------------------- */
#define SINE_SAMPLES        64            /* LUT points per cycle             */
#define SINE_FREQ_HZ        7.0f          /* excitation frequency (Hz)        */

/* DAC 1 Vpp centred at mid-rail 1.65 V  (VDDA = 3.3 V, 12-bit)
   mid = 4095 * 0.5        = 2047
   amp = 4095 * (0.5/3.3)  = 620   -> +/-0.5 V = 1 Vpp                     */
#define DAC_MID_COUNT       2047
#define DAC_AMP_COUNT       620

/* --- ADC capture --------------------------------------------------------- */
/* 16 complete cycles -> 16 * 64 = 1024 dual-word samples                   */
#define CYCLES_TO_CAPTURE   16
#define ADC_BUF_SIZE        (CYCLES_TO_CAPTURE * SINE_SAMPLES)  /* 1024     */

#define VREF                3.3f
#define ADC_FULL_SCALE      4095.0f

/* Safety: if either ADC reads this or above (~3.22 V) before the DAC
   starts, the probe is shorted — abort everything immediately               */
#define ADC_SAFETY_THRESHOLD  4000u

/* --- Conversion constants ------------------------------------------------ */
#define CONDUCTIVITY_SCALE  14.02f    /* (I_rms / V_rms) -> conductivity mS  */
#define SALINITY_SCALE      24.46f    /* conductivity mS -> salinity g/L     */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint16_t          sine_lut[SINE_SAMPLES];
static uint32_t          adc_buf[ADC_BUF_SIZE]; /* packed [ADC2(31:16)|ADC1(15:0)] */
static volatile uint8_t  adc_dma_done = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void               BuildSineLUT(void);
static ConductivityStatus SafetyCheck(void);
static float              ComputeRMS(const uint32_t *buf, uint32_t len, uint8_t shift);
ConductivityResult        Conductivity_Measure(void);
/* USER CODE END PFP */

/* ============================================================================
   USER CODE BEGIN 0
   ============================================================================ */

/**
  * @brief  Build a 64-point sine LUT scaled for 1 Vpp centred at mid-rail.
  */
static void BuildSineLUT(void)
{
    for (int i = 0; i < SINE_SAMPLES; i++) {
        float angle = 2.0f * (float)M_PI * i / SINE_SAMPLES;
        sine_lut[i] = (uint16_t)(DAC_MID_COUNT
                      + (int32_t)(DAC_AMP_COUNT * sinf(angle)));
    }
}

/**
  * @brief  Safety check — one poll-mode conversion BEFORE the DAC starts.
  *         If either ADC input is already near 3.3 V the circuit is faulty.
  *
  *         After the poll-mode conversion the ADCs are re-initialised so they
  *         are back in the timer-triggered DMA state CubeMX configured.
  *
  * @retval COND_OK, COND_FAULT_SHORT, or COND_FAULT_TIMEOUT
  */
static ConductivityStatus SafetyCheck(void)
{
    HAL_ADC_Start(&hadc2);
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        HAL_ADC_Stop(&hadc2);
        return COND_FAULT_TIMEOUT;
    }

    uint32_t raw1 = HAL_ADC_GetValue(&hadc1);
    uint32_t raw2 = HAL_ADC_GetValue(&hadc2);

    HAL_ADC_Stop(&hadc1);
    HAL_ADC_Stop(&hadc2);

    /* Restore ADCs to DMA-triggered state */
    MX_ADC1_Init();
    MX_ADC2_Init();

    if (raw1 >= ADC_SAFETY_THRESHOLD || raw2 >= ADC_SAFETY_THRESHOLD) {
        return COND_FAULT_SHORT;
    }

    return COND_OK;
}

/**
  * @brief  Compute AC-RMS of one channel from the interleaved dual-ADC buffer.
  *
  *         Dual-simultaneous DMA packs each word as:
  *           bits [15: 0] = ADC1 result  (shift =  0)
  *           bits [31:16] = ADC2 result  (shift = 16)
  *
  *         DC is removed by subtracting the mean first so the mid-rail DAC
  *         bias does not inflate the RMS result.
  *
  * @param  buf    Pointer to 32-bit dual-word buffer
  * @param  len    Number of 32-bit entries
  * @param  shift  0 for ADC1, 16 for ADC2
  * @retval AC RMS voltage in volts
  */
static float ComputeRMS(const uint32_t *buf, uint32_t len, uint8_t shift)
{
    /* Pass 1 — mean (DC component) */
    double sum = 0.0;
    for (uint32_t i = 0; i < len; i++) {
        uint16_t raw = (uint16_t)((buf[i] >> shift) & 0x0FFFu);
        sum += (double)raw;
    }
    double mean = sum / (double)len;

    /* Pass 2 — variance then RMS (AC only) */
    double sq_sum = 0.0;
    for (uint32_t i = 0; i < len; i++) {
        uint16_t raw = (uint16_t)((buf[i] >> shift) & 0x0FFFu);
        double ac = (double)raw - mean;
        sq_sum += ac * ac;
    }

    double rms_counts = sqrt(sq_sum / (double)len);
    return (float)(rms_counts * VREF / ADC_FULL_SCALE);
}

/**
  * @brief  DMA transfer-complete callback (overrides HAL weak implementation).
  *         Sets the flag that unblocks Conductivity_Measure().
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        adc_dma_done = 1;
    }
}

/**
  * @brief  Run one full conductivity / salinity measurement.
  *
  *         Steps:
  *           1. Safety check  — abort if ADC reads ~3.3 V with no excitation
  *           2. Start DAC sine wave + TIM6
  *           3. 50 ms settle
  *           4. Capture 1024 dual samples via DMA (TIM3 triggered)
  *           5. Stop all peripherals cleanly
  *           6. Compute RMS, conductivity, salinity
  *
  * @retval ConductivityResult  — always check .status before using values
  */
ConductivityResult Conductivity_Measure(void)
{
    ConductivityResult result;
    memset(&result, 0, sizeof(result));

    /* Step 1 — safety pre-check (no excitation yet) ----------------------- */
    result.status = SafetyCheck();
    if (result.status != COND_OK) {
        return result;
    }

    /* Step 2 — build LUT, start DAC + TIM6 -------------------------------- */
    BuildSineLUT();

    HAL_DAC_Start_DMA(&hdac,
                      DAC_CHANNEL_2,
                      (uint32_t *)sine_lut,
                      SINE_SAMPLES,
                      DAC_ALIGN_12B_R);

    HAL_TIM_Base_Start(&htim6);   /* triggers DAC at 64*7 = 448 Hz          */

    /* Step 3 — settle ------------------------------------------------------ */
    HAL_Delay(50);                /* ~half a 7 Hz cycle                      */

    /* Step 4 — start ADC DMA capture --------------------------------------- */
    adc_dma_done = 0;
    memset(adc_buf, 0, sizeof(adc_buf));

    HAL_ADC_Start(&hadc2);                              /* slave first        */
    HAL_ADC_Start_DMA(&hadc1, adc_buf, ADC_BUF_SIZE);  /* master + DMA       */
    HAL_TIM_Base_Start(&htim3);                         /* start ADC trigger  */

    /* Wait for DMA complete (3 s timeout) ---------------------------------- */
    uint32_t t_start = HAL_GetTick();
    while (!adc_dma_done) {
        if ((HAL_GetTick() - t_start) > 3000u) {
            result.status = COND_FAULT_TIMEOUT;
            goto cleanup;
        }
    }

    /* Step 5 — stop peripherals ------------------------------------------- */
    HAL_TIM_Base_Stop(&htim3);
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_ADC_Stop(&hadc2);

    HAL_TIM_Base_Stop(&htim6);
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2);

    /* Park PA5 at mid-rail then stop — prevents floating output glitch     */
    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_2, DAC_ALIGN_12B_R, DAC_MID_COUNT);
    HAL_DAC_Start(&hdac, DAC_CHANNEL_2);
    HAL_Delay(1);
    HAL_DAC_Stop(&hdac, DAC_CHANNEL_2);

    /* Step 6 — compute results -------------------------------------------- */
    float rms_I = ComputeRMS(adc_buf, ADC_BUF_SIZE, 0);   /* ADC1 IN9  — I  */
    float rms_V = ComputeRMS(adc_buf, ADC_BUF_SIZE, 16);  /* ADC2 IN14 — V  */

    result.adc1_rms_V = rms_I;
    result.adc2_rms_V = rms_V;

    if (rms_V < 1e-6f) {
        /* V channel is essentially zero — likely open circuit              */
        result.status = COND_FAULT_SHORT;
        return result;
    }

    result.conductivity_mS = (rms_I / rms_V) * CONDUCTIVITY_SCALE;
    result.salinity_gL     = result.conductivity_mS * SALINITY_SCALE;
    result.status          = COND_OK;
    return result;

cleanup:
    /* Emergency stop — shut everything down in the correct order           */
    HAL_TIM_Base_Stop(&htim3);
    HAL_TIM_Base_Stop(&htim6);
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_ADC_Stop(&hadc2);
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2);
    HAL_DAC_Stop(&hdac, DAC_CHANNEL_2);
    return result;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    /* Reset peripherals, init Flash and Systick --------------------------- */
    HAL_Init();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    /* Configure system clock ---------------------------------------------- */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */
    /* USER CODE END SysInit */

    /* Initialise all configured peripherals -------------------------------- */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C3_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    MX_USART6_UART_Init();
    MX_ADC2_Init();
    MX_DAC_Init();
    MX_TIM6_Init();
    MX_TIM3_Init();

    /* USER CODE BEGIN 2 */

    /* Run one measurement and transmit the result over USART2 ------------- */
    ConductivityResult result = Conductivity_Measure();

    char tx_buf[160];

    if (result.status == COND_OK) {
        snprintf(tx_buf, sizeof(tx_buf),
            "Conductivity: %.3f mS  |  Salinity: %.3f g/L"
            "  |  I_rms: %.4f V  |  V_rms: %.4f V\r\n",
            result.conductivity_mS,
            result.salinity_gL,
            result.adc1_rms_V,
            result.adc2_rms_V);
    }
    else if (result.status == COND_FAULT_SHORT) {
        snprintf(tx_buf, sizeof(tx_buf),
            "FAULT: Short / 3.3 V detected on ADC input before excitation"
            " — measurement aborted\r\n");
    }
    else {
        snprintf(tx_buf, sizeof(tx_buf),
            "FAULT: ADC DMA timeout — check TIM3 and DMA2 Stream0 config\r\n");
    }

    HAL_UART_Transmit(&huart2, (uint8_t *)tx_buf, strlen(tx_buf), HAL_MAX_DELAY);

    /* USER CODE END 2 */

    /* Infinite loop ------------------------------------------------------- */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */
        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
  * @brief  System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 168;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  Error handler — disables interrupts and halts.
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();
    while (1) {}
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
