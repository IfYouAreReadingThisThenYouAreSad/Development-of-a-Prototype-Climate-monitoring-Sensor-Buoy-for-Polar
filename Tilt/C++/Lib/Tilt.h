// tilt_sensor.h
#pragma once
#include "main.h"
#include <math.h>

typedef enum {
    TILT_OK = 0,
    TILT_ID_MISMATCH,
    TILT_SPI_ERROR
} TiltStatus;

/**
 * @brief High-precision SPI driver for the STMicroelectronics IIS2ICLX 2-Axis Inclinometer.
 * Implements hardware offset calibration, Exponential Moving Average (EMA) low-pass
 * filtering, and continuous full-duplex SPI transactions.
 */
class TiltSensor {
private:
    SPI_HandleTypeDef* spi_handle;
    GPIO_TypeDef*      cs_port;
    uint16_t           cs_pin;

    /* Exponential Moving Average (EMA) filter weight.
     * 0.2 means trust new data 20%, trust historical average 80%.
     * 1.0 disables filtering (full pass-through). */
    static constexpr float FILTER_ALPHA = 0.2f;

    /* Maximum allowable tilt before the EMI SIT coil data is considered invalid.
     * Set to 45.0 degrees based on coil operating constraints in ocean swells. */
    static constexpr float EMI_COIL_MAX_TILT_DEG = 45.0f;

    float offset_x_g;
    float offset_y_g;

    float filtered_x_g;
    float filtered_y_g;

    float current_x_rad;
    float current_y_rad;

    float current_x_deg;
    float current_y_deg;

    TiltStatus write_register(uint8_t reg_addr, uint8_t data);
    TiltStatus read_registers(uint8_t reg_addr, uint8_t* data_buffer, uint16_t length);

public:
    TiltSensor(SPI_HandleTypeDef* spi, GPIO_TypeDef* port, uint16_t pin);

    /**
     * @brief Initializes sensor connection, validates hardware ID, and sets operational ranges.
     * @return TILT_OK on success, or specific error code on failure.
     */
    TiltStatus init();

    /**
     * @brief Performs a one-time 50-sample calibration to find the static PCB resting error.
     * Must only be called when the hardware is perfectly level.
     */
    void calibrate_flat_offset();

    /**
     * @brief Pulls the latest raw axis data, applies the calibration offset,
     * runs the low-pass filter, and computes the trigonometric angles.
     */
    TiltStatus update();

    /**
     * @brief Evaluates if the buoy is currently tilted beyond the operational limits
     * of the EMI SIT coil.
     * @return true if tilted beyond safe limits (data corrupted), false if stable.
     */
    bool is_coil_tilt_compromised();

    float get_x_g();
    float get_y_g();
    float get_x_deg();
    float get_y_deg();
};
