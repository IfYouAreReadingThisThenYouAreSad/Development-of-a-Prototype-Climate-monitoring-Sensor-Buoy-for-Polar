/*
 * ltr390_driver.cpp
 *
 *  Created on: 3 Apr 2026
 *      Author: MuffadalTarawala
 *      Reviewer: ThomasNeedham
 *
 */

#include "ltr390_driver.hpp"

/**
 * @brief Pack 3 little-endian bytes into a 24-bit unsigned value.
 *
 * The LTR390 UVS/ALS data registers return 3 bytes, LSB first.
 * byte[0] = bits  7:0  (least significant)
 * byte[1] = bits 15:8
 * byte[2] = bits 23:16 (most significant)
 *
 * @param bytes  Pointer to exactly 3 bytes in little-endian order.
 * @return 24-bit value stored in the lower 24 bits of a uint32_t.
 */
uint32_t Ltr390Driver::pack_24bit_little_endian(const uint8_t bytes[3])
{
  return static_cast<uint32_t>(bytes[0])
       | (static_cast<uint32_t>(bytes[1]) << 8)
       | (static_cast<uint32_t>(bytes[2]) << 16);
}

/**
 * @brief Store the I2C handle for all subsequent register operations.
 *
 * The HAL handle already encapsulates the GPIO pins, clock speed, and
 * peripheral configuration set up by CubeMX — no additional parameters
 * are needed.
 *
 * @param hi2c_  Pointer to the initialised I2C handle (e.g. &hi2c1).
 */
Ltr390Driver::Ltr390Driver(I2C_HandleTypeDef *hi2c_)
  : hi2c(hi2c_)
{
}

/**
 * @brief Check that the sensor ACKs on the I2C bus.
 *
 * Asks the HAL to attempt 3 ACK polls with a 100 ms timeout each.
 * If the sensor does not respond it is likely unpowered or miswired.
 * Called internally by init() before writing any configuration registers.
 *
 * @return OK if the sensor responds, INVALID_ARGUMENT if hi2c is null,
 *         I2C_NO_ACKNOWLEDGMENT if the sensor does not respond.
 */
Ltr390Status Ltr390Driver::probe() const
{
  if (hi2c == nullptr) {
    return Ltr390Status::INVALID_ARGUMENT;
  }

  const HAL_StatusTypeDef hal_status = HAL_I2C_IsDeviceReady(hi2c, i2c_device_address, 3, 100);

  return (hal_status == HAL_OK) ? Ltr390Status::OK : Ltr390Status::I2C_NO_ACKNOWLEDGMENT;
}

/**
 * @brief Write a single byte value to a sensor register.
 *
 * Builds a 2-byte I2C payload — first byte is the register address,
 * second byte is the value to write — then transmits both in one transaction.
 *
 * @param reg  Register address to write to.
 * @param val  Value to write into the register.
 * @return OK on success, I2C_TRANSMIT_ERROR if the HAL transaction fails.
 */
Ltr390Status Ltr390Driver::write_reg8(uint8_t reg, uint8_t val) const
{
  uint8_t transmit_buffer[2] = {reg, val};

  if (HAL_I2C_Master_Transmit(hi2c, i2c_device_address, transmit_buffer, 2, 100) != HAL_OK) {
    return Ltr390Status::I2C_TRANSMIT_ERROR;
  }

  return Ltr390Status::OK;
}

/**
 * @brief Read a single byte from a sensor register.
 *
 * Two-phase transaction: first transmits the register address so the sensor
 * knows what to return, then reads one byte back.
 *
 * @param reg  Register address to read from.
 * @param val  Output — byte read from the register.
 * @return OK on success, I2C_TRANSMIT_ERROR or I2C_RECEIVE_ERROR on failure.
 */
Ltr390Status Ltr390Driver::read_reg8(uint8_t reg, uint8_t &val) const
{
  // Phase 1: tell the sensor which register to read.
  if (HAL_I2C_Master_Transmit(hi2c, i2c_device_address, &reg, 1, 100) != HAL_OK) {
    return Ltr390Status::I2C_TRANSMIT_ERROR;
  }

  // Phase 2: clock out one byte from that register.
  if (HAL_I2C_Master_Receive(hi2c, i2c_device_address, &val, 1, 100) != HAL_OK) {
    return Ltr390Status::I2C_RECEIVE_ERROR;
  }

  return Ltr390Status::OK;
}

/**
 * @brief Read multiple consecutive bytes starting from a register address.
 *
 * The LTR390 auto-increments the register pointer after each byte, so a
 * single transmit + receive transaction reads a contiguous block.
 * Used for UVS (0x10–0x12) and ALS (0x0D–0x0F) data, which are always
 * 3 bytes each.
 *
 * @param start_reg  Address of the first register to read.
 * @param buffer     Output buffer — must be at least 'length' bytes.
 * @param length     Number of bytes to read.
 * @return OK on success, I2C_TRANSMIT_ERROR or I2C_RECEIVE_ERROR on failure.
 */
Ltr390Status Ltr390Driver::read_reg_n(uint8_t start_reg, uint8_t *buffer, uint16_t length) const
{
  // Phase 1: send the starting register address.
  if (HAL_I2C_Master_Transmit(hi2c, i2c_device_address, &start_reg, 1, 100) != HAL_OK) {
    return Ltr390Status::I2C_TRANSMIT_ERROR;
  }

  // Phase 2: read 'length' bytes back from that register onwards.
  if (HAL_I2C_Master_Receive(hi2c, i2c_device_address, buffer, length, 100) != HAL_OK) {
    return Ltr390Status::I2C_RECEIVE_ERROR;
  }

  return Ltr390Status::OK;
}

/**
 * @brief Select ALS or UVS measurement mode and ensure the sensor is enabled.
 *
 * The LTR390 measures either visible light (ALS) or UV (UVS) depending on
 * the MAIN_CTRL register bits. MAIN_CTRL_ENABLE is always set to keep
 * measurements running. MAIN_CTRL_UVS_MODE is added only for UV mode.
 *
 * Note: switching mode requires a fresh integration cycle before new data
 * is available. wait_data_ready() handles this by polling DATA_READY.
 *
 * @param mode  AMBIENT_LIGHT or ULTRAVIOLET.
 * @return OK on success, I2C_TRANSMIT_ERROR if the register write fails.
 */
Ltr390Status Ltr390Driver::set_mode(Mode mode) const
{
  uint8_t control_register_value = MAIN_CTRL_ENABLE;

  if (mode == Mode::ULTRAVIOLET) {
    control_register_value |= MAIN_CTRL_UVS_MODE;
  }

  return write_reg8(REG_MAIN_CTRL, control_register_value);
}

/**
 * @brief Poll MAIN_STATUS until DATA_READY is set or the timeout expires.
 *
 * Polling is used in place of interrupts to keep bring-up simple and avoid
 * EXTI/ISR complexity. The poll interval is 5 ms to avoid hammering the I2C
 * bus while still catching the ready flag promptly.
 *
 * DATA_READY_TIMEOUT_MS must be long enough to cover the configured
 * integration time plus measurement rate — at 20-bit / 400 ms + 500 ms rate,
 * the default 1000 ms timeout is sufficient.
 *
 * @return OK when DATA_READY is set, I2C_RECEIVE_ERROR if a read fails,
 *         TIMEOUT if the bit never sets within DATA_READY_TIMEOUT_MS.
 */
Ltr390Status Ltr390Driver::wait_data_ready() const
{
  const uint32_t start_tick = HAL_GetTick();

  while ((HAL_GetTick() - start_tick) < DATA_READY_TIMEOUT_MS) {
    uint8_t status_register = 0;

    if (read_reg8(REG_MAIN_STATUS, status_register) != Ltr390Status::OK) {
      return Ltr390Status::I2C_RECEIVE_ERROR;
    }

    // Bit 3 of MAIN_STATUS is set by the sensor when a fresh measurement is ready.
    if ((status_register & MAIN_STATUS_DATA_READY) != 0u) {
      return Ltr390Status::OK;
    }

    HAL_Delay(5);
  }

  return Ltr390Status::TIMEOUT;
}

/**
 * @brief Initialise the sensor with default configuration.
 *
 * Sequence:
 *   1. probe()         — confirms the sensor is reachable on I2C.
 *   2. HAL_Delay(10)   — allows the sensor to settle after power-on.
 *   3. REG_MEAS_RATE   — sets resolution and measurement rate.
 *   4. REG_GAIN        — sets the photodiode gain.
 *   5. set_mode()      — enables the sensor in UV measurement mode.
 *
 * Note: soft reset was removed because writing the reset bit causes the
 * sensor to briefly drop off I2C, returning a transmit error even though
 * the reset itself succeeds. The sensor powers up in a safe default state
 * so the reset is not needed.
 *
 * @return OK on success, or the first error code encountered.
 */
Ltr390Status Ltr390Driver::init()
{
  if (probe() != Ltr390Status::OK) {
    return Ltr390Status::I2C_NO_ACKNOWLEDGMENT;
  }

  HAL_Delay(10);

  if (write_reg8(REG_MEAS_RATE, MEAS_RATE_DEFAULT) != Ltr390Status::OK) {
    return Ltr390Status::RESOLUTION_SETUP_ERROR;
  }

  if (write_reg8(REG_GAIN, GAIN) != Ltr390Status::OK) {
    return Ltr390Status::GAIN_SETUP_ERROR;
  }

  return set_mode(Mode::ULTRAVIOLET);
}

/**
 * @brief Read raw UVS counts from the sensor into uv_counts.
 *
 * Switches to UV mode, waits for a fresh measurement via wait_data_ready(),
 * then reads 3 bytes from REG_UVS_DATA0 (0x10–0x12) and packs them into
 * a single 24-bit value stored in uv_counts.
 *
 * @return OK on success, or the first error code encountered.
 */
Ltr390Status Ltr390Driver::read_uvs_counts()
{
  Ltr390Status status = set_mode(Mode::ULTRAVIOLET);
  if (status != Ltr390Status::OK) {
    return Ltr390Status::UV_MODE_SWITCH_ERROR;
  }

  status = wait_data_ready();
  if (status != Ltr390Status::OK) {
    return Ltr390Status::TIMEOUT;
  }

  uint8_t raw_bytes[3] = {0, 0, 0};
  status = read_reg_n(REG_UVS_DATA0, raw_bytes, 3);
  if (status != Ltr390Status::OK) {
    return status;
  }

  uv_counts = pack_24bit_little_endian(raw_bytes);
  return Ltr390Status::OK;
}

/**
 * @brief Read raw ALS counts from the sensor into als_counts.
 *
 * Switches to ALS mode, waits for a fresh measurement via wait_data_ready(),
 * then reads 3 bytes from REG_ALS_DATA0 (0x0D–0x0F) and packs them into
 * a single 24-bit value stored in als_counts.
 *
 * ALS readings are useful alongside UV to detect window obstruction or
 * shading — if both are low simultaneously, the sensor path may be blocked.
 *
 * @return OK on success, or the first error code encountered.
 */
Ltr390Status Ltr390Driver::read_als_counts()
{
  Ltr390Status status = set_mode(Mode::AMBIENT_LIGHT);
  if (status != Ltr390Status::OK) {
    return Ltr390Status::ALS_MODE_SWITCH_ERROR;
  }

  status = wait_data_ready();
  if (status != Ltr390Status::OK) {
    return Ltr390Status::TIMEOUT;
  }

  uint8_t raw_bytes[3] = {0, 0, 0};
  status = read_reg_n(REG_ALS_DATA0, raw_bytes, 3);
  if (status != Ltr390Status::OK) {
    return status;
  }

  als_counts = pack_24bit_little_endian(raw_bytes);
  return Ltr390Status::OK;
}

/**
 * @brief Read raw UVS counts and convert them to UV Index.
 *
 * Calls read_uvs_counts() to acquire a fresh measurement, then applies:
 *
 *   UVI = (uv_counts / COUNTS_PER_UVI) * WINDOW_FACTOR
 *
 * COUNTS_PER_UVI is valid only for the reference settings (18x gain,
 * 20-bit resolution, 400 ms integration). If GAIN or MEAS_RATE_DEFAULT
 * are changed, COUNTS_PER_UVI must be recalculated from the datasheet.
 *
 * On any read failure uv_index is set to 0 so the caller always gets a
 * defined value regardless of the error path taken.
 *
 * TODO: consider taking an average over multiple readings for better
 * noise rejection — this could be done here or in the calling code.
 *
 * @return OK on success, or the error code from read_uvs_counts().
 */
Ltr390Status Ltr390Driver::calculate_uv_index()
{
  // WINDOW_FACTOR must be positive — a zero or negative value would produce
  // a nonsense UVI reading or divide-by-zero in a future refactor.
  if (WINDOW_FACTOR <= 0.0f) {
    return Ltr390Status::INVALID_ARGUMENT;
  }

  Ltr390Status uv_status = read_uvs_counts();
  if (uv_status != Ltr390Status::OK) {
    uv_index = 0.0f;
    return uv_status;
  }

  // uv_counts == 0 means complete darkness — UVI is zero by definition.
  if (uv_counts == 0) {
    uv_index = 0.0f;
    return Ltr390Status::OK;
  }

  uv_index = (static_cast<float>(uv_counts) / COUNTS_PER_UVI) * WINDOW_FACTOR;
  return Ltr390Status::OK;
}

/**
 * @brief Read raw ALS counts and convert them to lux.
 *
 * Calls read_als_counts() to acquire a fresh measurement, then applies
 * the datasheet formula:
 *
 *   Lux = (ALS_SENSITIVITY * als_counts) / (ALS_GAIN * ALS_INTEGRATION_TIME)
 *
 * ALS_GAIN and ALS_INTEGRATION_TIME are derived automatically at compile
 * time from GAIN and MEAS_RATE_DEFAULT — no manual update needed if those
 * configuration constants are changed.
 *
 * On any read failure lux is set to 0 so the caller always gets a defined
 * value regardless of the error path taken.
 *
 * @return OK on success, or the error code from read_als_counts().
 */
Ltr390Status Ltr390Driver::calculate_lux()
{
  Ltr390Status als_status = read_als_counts();
  if (als_status != Ltr390Status::OK) {
    lux = 0.0f;
    return als_status;
  }

  // als_counts == 0 means complete darkness — lux is zero by definition.
  if (als_counts == 0) {
    lux = 0.0f;
    return Ltr390Status::OK;
  }

  lux = (ALS_SENSITIVITY * static_cast<float>(als_counts))
        / (ALS_GAIN * ALS_INTEGRATION_TIME);

  return Ltr390Status::OK;
}
