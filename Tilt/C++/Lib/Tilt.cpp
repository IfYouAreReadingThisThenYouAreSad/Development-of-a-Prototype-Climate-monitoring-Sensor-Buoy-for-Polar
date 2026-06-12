// tilt_sensor.cpp
#include "tilt_sensor.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

TiltSensor::TiltSensor(SPI_HandleTypeDef* spi, GPIO_TypeDef* port, uint16_t pin)
    : spi_handle(spi), cs_port(port), cs_pin(pin),
      offset_x_g(0.0f), offset_y_g(0.0f),
      filtered_x_g(0.0f), filtered_y_g(0.0f),
      current_x_rad(0.0f), current_y_rad(0.0f),
      current_x_deg(0.0f), current_y_deg(0.0f)
{}

// ─── Private helpers ─────────────────────────────────────────────────────────

/**
 * @brief Writes a single byte to a target register via SPI.
 */
TiltStatus TiltSensor::write_register(uint8_t reg_addr, uint8_t data)
{
    uint8_t tx_buffer[2] = { reg_addr, data }; // MSB is 0 for write operations

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(spi_handle, tx_buffer, 2, 100);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    return (status == HAL_OK) ? TILT_OK : TILT_SPI_ERROR;
}

/**
 * @brief Reads a sequence of bytes via continuous full-duplex SPI.
 * Uses TransmitReceive to ensure the SPI clock does not pause, preventing
 * bit-shift errors.
 */
TiltStatus TiltSensor::read_registers(uint8_t reg_addr, uint8_t* data_buffer, uint16_t length)
{
    if (length > 5) return TILT_SPI_ERROR; // Buffer safety guard

    uint8_t tx_buffer[6] = { 0 }; // Address byte + up to 5 dummy bytes to generate clock
    uint8_t rx_buffer[6] = { 0 };

    tx_buffer[0] = reg_addr | 0x80; // Set MSB to 1 for read operations

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(spi_handle, tx_buffer, rx_buffer, length + 1, 100);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    if (status != HAL_OK) return TILT_SPI_ERROR;

    // Shift received data by 1 to discard the junk byte clocked in during address transmission
    for (uint16_t i = 0; i < length; i++) {
        data_buffer[i] = rx_buffer[i + 1];
    }
    return TILT_OK;
}

// ─── Public API ──────────────────────────────────────────────────────────────

TiltStatus TiltSensor::init()
{
    uint8_t who_am_i = 0;
    if (read_registers(0x0F, &who_am_i, 1) != TILT_OK) return TILT_SPI_ERROR;
    if (who_am_i != 0x6B) return TILT_ID_MISMATCH;

    /* CTRL1_XL (0x10):
     * Bits [7:4] = 0001  → 12.5 Hz ODR
     * Bits [3:2] = 11    → ±2g full-scale (FS_XL[1:0] = 11 → 0.061 mg/LSB, non-standard encoding)
     * Bits [1:0] = 00    → LPF2 default; must be 0
     * Binary: 0001 1100 = 0x1C
     */
    if (write_register(0x10, 0x1C) != TILT_OK) return TILT_SPI_ERROR;

    // CTRL3_C (0x12): 0x44 enables Block Data Update (BDU) and register auto-increment
    if (write_register(0x12, 0x44) != TILT_OK) return TILT_SPI_ERROR;

    return TILT_OK;
}

void TiltSensor::calibrate_flat_offset()
{
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    uint8_t raw_data[4];

    for (int i = 0; i < 50; i++) {
        if (read_registers(0x28, raw_data, 4) == TILT_OK) {
            int16_t raw_x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
            int16_t raw_y = (int16_t)((raw_data[3] << 8) | raw_data[2]);

            // Sensitivity: 0.061 mg/LSB for ±2g scale
            sum_x += (raw_x * 0.061f) / 1000.0f;
            sum_y += (raw_y * 0.061f) / 1000.0f;
        }
        HAL_Delay(10);
    }

    offset_x_g = sum_x / 50.0f;
    offset_y_g = sum_y / 50.0f;
}

TiltStatus TiltSensor::update()
{
    uint8_t raw_data[4];
    if (read_registers(0x28, raw_data, 4) != TILT_OK) return TILT_SPI_ERROR;

    int16_t raw_x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
    int16_t raw_y = (int16_t)((raw_data[3] << 8) | raw_data[2]);

    // 1. Convert LSB → g (0.061 mg/LSB for ±2g scale) and subtract resting offset
    float raw_x_g = ((raw_x * 0.061f) / 1000.0f) - offset_x_g;
    float raw_y_g = ((raw_y * 0.061f) / 1000.0f) - offset_y_g;

    // 2. Exponential Moving Average filter
    filtered_x_g = (raw_x_g * FILTER_ALPHA) + (filtered_x_g * (1.0f - FILTER_ALPHA));
    filtered_y_g = (raw_y_g * FILTER_ALPHA) + (filtered_y_g * (1.0f - FILTER_ALPHA));

    // 3. Domain clamp — guard arcsin against values outside [-1, 1]
    float clamped_x = filtered_x_g;
    if (clamped_x >  1.0f) clamped_x =  1.0f;
    if (clamped_x < -1.0f) clamped_x = -1.0f;

    float clamped_y = filtered_y_g;
    if (clamped_y >  1.0f) clamped_y =  1.0f;
    if (clamped_y < -1.0f) clamped_y = -1.0f;

    // 4. Trigonometric conversion
    current_x_rad = asinf(clamped_x);
    current_y_rad = asinf(clamped_y);

    current_x_deg = current_x_rad * 57.2957795f;
    current_y_deg = current_y_rad * 57.2957795f;

    return TILT_OK;
}

bool TiltSensor::is_coil_tilt_compromised()
{
    return (fabsf(current_x_deg) > EMI_COIL_MAX_TILT_DEG ||
            fabsf(current_y_deg) > EMI_COIL_MAX_TILT_DEG);
}

// ─── Accessors ───────────────────────────────────────────────────────────────

float TiltSensor::get_x_g()   { return filtered_x_g;   }
float TiltSensor::get_y_g()   { return filtered_y_g;   }
float TiltSensor::get_x_deg() { return current_x_deg;  }
float TiltSensor::get_y_deg() { return current_y_deg;  }
