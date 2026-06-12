/*
 * BatteryMonitor.h
 *
 *  Created on: 27 Mar 2026
 *      Author: BasheerAbdel-Nour
 *      Reviewer: ThomasNeedham
 */

#ifndef INC_BATTERYMONITOR_H_
#define INC_BATTERYMONITOR_H_

#include "stm32f4xx_hal.h"

/**
 * @brief Return codes for BatteryMonitor operations.
 */
typedef enum {
    BATTERY_OK = 0,       /**< Operation completed successfully */
    BATTERY_I2C_ERROR,    /**< I2C communication failure with INA219 */
    BATTERY_DATA_FAULT    /**< Sensor returned implausible data */
} BatteryStatus;

/**
 * @brief Monitors a 2S Li-Ion battery pack using an INA219 over I2C.
 *
 * Combines coulomb counting with a voltage-curve anchor to estimate
 * state-of-charge (SOC). Cold temperature derating is applied dynamically
 * at runtime to account for reduced capacity at low temperatures.
 */
class BatteryMonitor {
private:

    /**
     * @brief A single point on a voltage-to-SOC discharge curve.
     */
    struct SocPoint {
        float voltage;     /**< Cell voltage in volts */
        float percentage;  /**< Corresponding SOC percentage (0–100) */
    };

    static constexpr uint8_t  CURVE_POINTS       = 6;      /**< Number of points in each discharge curve */
    static constexpr uint16_t INA219_I2C_ADDRESS = 0x80;   /**< 8-bit I2C address of the INA219 sensor */

    static constexpr float RESTING_CURRENT_MA    = 50.0f;  /**< Current threshold (mA) below which battery is considered at rest */
    static constexpr float DRIFT_CORRECTION_SPEED = 0.01f; /**< Complementary filter coefficient — nudges 1% toward voltage anchor per update */

    /** @brief Discharge curve at +25°C (room temperature, full capacity) */
    static constexpr SocPoint CURVE_25C[CURVE_POINTS] = {
        {8.4f, 100.0f}, {7.6f, 75.0f}, {7.3f, 50.0f},
        {7.1f, 25.0f},  {6.7f,  5.0f}, {5.5f,  0.0f}
    };

    /** @brief Discharge curve at -20°C (minimum operating temperature, derated capacity) */
    static constexpr SocPoint CURVE_N20C[CURVE_POINTS] = {
        {7.9f, 100.0f}, {7.0f, 75.0f}, {6.8f, 50.0f},
        {6.4f, 25.0f},  {6.2f,  5.0f}, {4.5f,  0.0f}
    };

    I2C_HandleTypeDef* i2c_handle;                  /**< Pointer to the HAL I2C handle used to communicate with the INA219 */
    SocPoint active_voltage_curve[CURVE_POINTS];    /**< Temperature-interpolated discharge curve used for the current update cycle */
    float active_capacity_mah;                      /**< Temperature-derated battery capacity in mAh */
    float total_mah_consumed;                       /**< Coulomb-counted charge consumed since last full charge, in mAh */
    uint32_t last_time_ms;                          /**< HAL tick timestamp of the last update, used for delta-time integration */
    float current_percentage;                       /**< Last calculated SOC percentage (0–100) */
    float current_voltage;                          /**< Last measured bus voltage in volts */
    float current_ma;                               /**< Last measured current in milliamps (negative = charging) */

    /**
     * @brief Derates battery capacity and interpolates the active voltage curve for a given temperature.
     *
     * Linearly interpolates between CURVE_25C and CURVE_N20C based on how close
     * the current temperature is to each extreme. Also scales active_capacity_mah
     * from 5200mAh (25°C) down to ~3950mAh (-20°C).
     *
     * @param current_temp_c Current battery temperature in degrees Celsius. Clamped to [-20, +25].
     */
    void apply_cold_derating(float current_temp_c);

    /**
     * @brief Estimates SOC by interpolating the active voltage curve for a measured voltage.
     *
     * Performs a linear search through active_voltage_curve and interpolates between
     * the two surrounding points. Used on startup and as the voltage anchor during resting.
     *
     * @param measured_voltage Bus voltage reading in volts.
     * @return Estimated SOC as a percentage (0.0–100.0).
     */
    float estimate_starting_soc(float measured_voltage);

public:

    /**
     * @brief Constructs a BatteryMonitor and zero-initialises all state.
     *
     * @param handle Pointer to an initialised HAL I2C handle connected to the INA219.
     */
    BatteryMonitor(I2C_HandleTypeDef* handle);

    /**
     * @brief Initialises the monitor by reading the current voltage and estimating starting SOC.
     *
     * Must be called once after construction and before the first call to update().
     * Applies cold derating for the given temperature, reads the INA219 bus voltage register,
     * and sets total_mah_consumed to match the estimated starting SOC.
     *
     * @param current_temp_c Battery temperature at startup in degrees Celsius.
     * @return BATTERY_OK on success, BATTERY_I2C_ERROR if the INA219 could not be read.
     */
    BatteryStatus init(float current_temp_c);

    /**
     * @brief Updates SOC by integrating current draw and applying a voltage anchor correction.
     *
     * Should be called periodically (e.g. every few seconds) from the main loop.
     * Performs the following steps:
     *  1. Recalculates cold derating for the current temperature.
     *  2. Reads shunt current from INA219 register 0x01.
     *  3. Reads bus voltage from INA219 register 0x02.
     *  4. Integrates current over elapsed time (coulomb counting).
     *  5. If current is below RESTING_CURRENT_MA, applies a complementary filter
     *     to gently pull the integrated SOC toward the voltage-curve estimate.
     *  6. Recalculates current_percentage and clamps to [0, 100].
     *
     * @param current_temp_c Current battery temperature in degrees Celsius.
     * @return BATTERY_OK on success, BATTERY_I2C_ERROR if either I2C read fails.
     */
    BatteryStatus update(float current_temp_c);

    /**
     * @brief Returns the last calculated state-of-charge percentage.
     * @return SOC as a float in the range [0.0, 100.0].
     */
    float get_percentage();

    /**
     * @brief Returns the last measured battery bus voltage.
     * @return Voltage in volts.
     */
    float get_voltage();

    /**
     * @brief Returns the last measured current draw.
     * @return Current in milliamps. Negative values indicate charging.
     */
    float get_current();
};

#endif /* INC_BATTERYMONITOR_H_ */
