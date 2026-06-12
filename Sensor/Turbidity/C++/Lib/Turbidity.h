#pragma once

#include <stdint.h>

extern "C" {
#include "stm32f4xx_hal.h"
}

class TurbiditySensor {
public:
    /**
     * @brief Construct a TurbiditySensor
     * @param controlPort GPIO port for the LED control pin (e.g. GPIOA).
     * @param controlPin  GPIO pin mask for the LED control pin
     *                    (e.g. GPIO_PIN_5). HIGH = LED on, LOW = LED off.
     */
    TurbiditySensor(ADC_HandleTypeDef* hadc,
                    GPIO_TypeDef*       controlPort,
                    uint16_t            controlPin);

    /**
     * @brief Perform a full differential turbidity measurement.
     *
     * Sequence:
     *   1. Sample 100x ADC with LED off  -> V_ambient
     *   2. LED on + 20 ms settle
     *   3. Sample 100x ADC with LED on   -> V_excited = V_ambient + V_backscatter
     *   4. LED off
     *   5. V_backscatter = V_excited - V_ambient  (cancels ambient noise)
     *   6. Map V_backscatter -> NTU via calibration curve
     *
     * @return Turbidity in NTU (0.0f if below noise floor).
     */
    float readTurbidity();
    float readBackscatterVolts(float& vAmbientOut, float& vExcitedOut);;
    /**
     * @brief Convert a raw 12-bit ADC count to millivolts.
     *        Useful during calibration. Assumes Vref = 3300 mV.
     *
     * @param counts Raw ADC value (0–4095).
     * @return Voltage in millivolts.
     */
    static float countsToMillivolts(float counts);

private:
    // --- Hardware handles ---
    ADC_HandleTypeDef* _hadc;
    GPIO_TypeDef*      _controlPort;
    uint16_t           _controlPin;

    // --- Configuration constants ---
    static constexpr uint32_t NUM_SAMPLES   = 100;
    static constexpr uint32_t LED_SETTLE_MS = 20;
    static constexpr float    VREF_MV       = 3300.0f;
    static constexpr uint32_t ADC_MAX_COUNT = 4095;   // 12-bit


    // --- Private methods ---

    /**
     * @brief Drive control pin HIGH and wait LED_SETTLE_MS for the LED
     *        to reach steady-state brightness.
     */
    void ledOn();

    /**
     * @brief Drive control pin LOW (LED off).
     */
    void ledOff();

    /**
     * @brief Collect NUM_SAMPLES ADC readings via polling and return
     *        their arithmetic mean as a float.
     *
     * @return Mean ADC count over NUM_SAMPLES readings.
     */
    float sampleMean();

    /**
     * @brief Map a raw backscatter ADC count to NTU.
     *        Replace the body with your empirically derived curve.
     *
     * @param vBackscatter Differential ADC counts (V_excited − V_ambient).
     * @return Turbidity in NTU.
     */
    float backscatterToNtu(float vBackscatter) const;
};
