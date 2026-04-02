/*
 * BatteryMonitor.cpp
 *
 *  Created on: 27 Mar 2026
 *      Author: thomasneedham
 */

#include "BatteryMonitor.h"


constexpr BatteryMonitor::SocPoint BatteryMonitor::CURVE_25C[BatteryMonitor::CURVE_POINTS];
constexpr BatteryMonitor::SocPoint BatteryMonitor::CURVE_N20C[BatteryMonitor::CURVE_POINTS];





    /* Derates battery capacity and interpolates between two known temperature curves. */
    void BatteryMonitor::apply_cold_derating(float current_temp_c) {
        // Known curve at room temperature


        if (current_temp_c > 25.0f) {
            current_temp_c = 25.0f;
        } else if (current_temp_c < -20.0f) {
            current_temp_c = -20.0f;
        }

        // 0.0 at 25°C, 1.0 at -20°C
        float severity = (25.0f - current_temp_c) / 45.0f;

        // Scale total capacity: Drops from 5200mAh down to ~3950mAh
        active_capacity_mah = 5200.0f - (1250.0f * severity);

        // Interpolate the voltage points between the warm and cold curves
        for(uint8_t i = 0; i < CURVE_POINTS; i++) {
            active_voltage_curve[i].percentage = CURVE_25C[i].percentage;

            float v_difference = CURVE_25C[i].voltage - CURVE_N20C[i].voltage;
            active_voltage_curve[i].voltage = CURVE_25C[i].voltage - (v_difference * severity);
        }
    }

    float BatteryMonitor::estimate_starting_soc(float measured_voltage) {
        if (measured_voltage >= active_voltage_curve[0].voltage) return 100.0f;
        else if(measured_voltage <= active_voltage_curve[CURVE_POINTS - 1].voltage) return 0.0f;

        for (uint8_t i = 0; i < CURVE_POINTS - 1; i++) {
            if (measured_voltage <= active_voltage_curve[i].voltage && measured_voltage > active_voltage_curve[i+1].voltage) {
                float v_range = active_voltage_curve[i].voltage - active_voltage_curve[i+1].voltage;
                float p_range = active_voltage_curve[i].percentage - active_voltage_curve[i+1].percentage;
                float v_offset = measured_voltage - active_voltage_curve[i+1].voltage;

                return active_voltage_curve[i+1].percentage + ((v_offset / v_range) * p_range);
            }
        }
        return 0.0f;
    }


    BatteryMonitor::BatteryMonitor(I2C_HandleTypeDef* handle) :
        i2c_handle(handle),
        active_capacity_mah(0.0f),
        total_mah_consumed(0.0f),
        last_time_ms(0),
        current_percentage(100.0f),
        current_voltage(0.0f),
        current_ma(0.0f) {}

    BatteryStatus BatteryMonitor::init(float current_temp_c) {
        apply_cold_derating(current_temp_c);

        uint8_t raw_data[2];
        if (HAL_I2C_Mem_Read(i2c_handle, INA219_I2C_ADDRESS, 0x02, 1, raw_data, 2, 100) != HAL_OK) {
            return BATTERY_I2C_ERROR;
        }

        uint16_t initial_reg = (raw_data[0] << 8) | raw_data[1];
        current_voltage = (initial_reg >> 3) * 0.004f;

        float starting_percentage = estimate_starting_soc(current_voltage);
        total_mah_consumed = active_capacity_mah * ((100.0f - starting_percentage) / 100.0f);
        current_percentage = starting_percentage;

        last_time_ms = HAL_GetTick();

        return BATTERY_OK;
    }

    /* Added current_temp_c parameter to dynamically adjust capacity during runtime */
    BatteryStatus BatteryMonitor::update(float current_temp_c) {
        // 1. Recalibrate battery physics based on immediate temperature
        apply_cold_derating(current_temp_c);

        uint8_t raw_data[2];

        // 2. Read Current
        if (HAL_I2C_Mem_Read(i2c_handle, INA219_I2C_ADDRESS, 0x01, 1, raw_data, 2, 100) != HAL_OK) {
            return BATTERY_I2C_ERROR;
        }
        int16_t shunt_reg = (raw_data[0] << 8) | raw_data[1];
        current_ma = shunt_reg * 0.1f;

        // 3. Read Voltage
        if (HAL_I2C_Mem_Read(i2c_handle, INA219_I2C_ADDRESS, 0x02, 1, raw_data, 2, 100) != HAL_OK) {
            return BATTERY_I2C_ERROR;
        }
        uint16_t bus_reg = (raw_data[0] << 8) | raw_data[1];
        current_voltage = (bus_reg >> 3) * 0.004f;

        // 4. Coulomb Counting (Integration)
        uint32_t current_time = HAL_GetTick();
        float time_elapsed_hours = (current_time - last_time_ms) / 3600000.0f;
        last_time_ms = current_time;

        total_mah_consumed += (current_ma * time_elapsed_hours);

        // 5. THE VOLTAGE ANCHOR (Drift Correction)
        // If current is low (buoy is asleep/idle), voltage is stable and not sagging.
        // We gently pull the integrated consumption towards the voltage-curve reality.
        if (current_ma > -RESTING_CURRENT_MA && current_ma < RESTING_CURRENT_MA) {
            float voltage_soc = estimate_starting_soc(current_voltage);
            float anchored_mah_consumed = active_capacity_mah * ((100.0f - voltage_soc) / 100.0f);

            // Complementary filter: 99% trusting the integration, 1% trusting the voltage
            total_mah_consumed = (total_mah_consumed * (1.0f - DRIFT_CORRECTION_SPEED)) +
                                 (anchored_mah_consumed * DRIFT_CORRECTION_SPEED);
        }

        // 6. Update final percentage and bounds check
        float remaining_mah = active_capacity_mah - total_mah_consumed;
        current_percentage = (remaining_mah / active_capacity_mah) * 100.0f;

        if (current_percentage > 100.0f) {
            current_percentage = 100.0f;
            total_mah_consumed = 0.0f;
        } else if (current_percentage < 0.0f) {
            current_percentage = 0.0f;
            total_mah_consumed = active_capacity_mah;
        }

        return BATTERY_OK;
    }

    // --- Getter Functions ---
    float BatteryMonitor::get_percentage() { return current_percentage; }
    float BatteryMonitor::get_voltage() { return current_voltage; }
    float BatteryMonitor::get_current() { return current_ma; }

