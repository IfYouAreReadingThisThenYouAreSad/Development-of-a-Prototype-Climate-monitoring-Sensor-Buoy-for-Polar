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
 * Why: The LTR390 UVS/ALS data registers return 3 bytes (LSB first). We keep this
 * helper local to this translation unit so callers cannot misuse it directly.
 *
 * @param b Pointer to 3 bytes in little-endian order: b[0]=LSB, b[2]=MSB.
 * @return 24-bit value stored in a 32-bit container.
 */
uint32_t Ltr390Driver::pack_24bit_little_endian(const uint8_t bytes[3])
{
  // Combine 3 bytes (LSB first) into one 24-bit integer stored in a 32-bit variable.
  // byte[0] = lowest byte, byte[1] = middle byte, byte[2] = highest byte.
  return static_cast<uint32_t>(bytes[0])
       | (static_cast<uint32_t>(bytes[1]) << 8)
       | (static_cast<uint32_t>(bytes[2]) << 16);
}

Ltr390Driver::Ltr390Driver(I2C_HandleTypeDef *hi2c_,
                           GPIO_TypeDef *sda_port_,
                           uint16_t sda_pin_,
                           GPIO_TypeDef *scl_port_,
                           uint16_t scl_pin_)
  : hi2c(hi2c_),
    sda_port(sda_port_),
    sda_pin(sda_pin_),
    scl_port(scl_port_),
    scl_pin(scl_pin_)
{
  // GPIO pin references are stored for future I2C bus-recovery support.
  // CubeMX controls these pins directly during normal operation.
}

Ltr390Status Ltr390Driver::probe() const
{
  // Guard: if no I2C handle was provided at construction, we cannot communicate.
  if (hi2c == nullptr) {
    return Ltr390Status::INVALID_ARGUMENT;
  }



  // Ask the HAL to attempt 3 ACK checks with a 100 ms timeout each.
  // If the sensor does not respond, it is likely unpowered or miswired.
  const HAL_StatusTypeDef hal_status = HAL_I2C_IsDeviceReady(hi2c, i2c_device_address, 3, 100);

  return (hal_status == HAL_OK) ? Ltr390Status::OK : Ltr390Status::I2C_NO_ACKNOWLEDGMENT;
}

Ltr390Status Ltr390Driver::write_reg8(uint8_t reg, uint8_t val) const
{
  // Build a 2-byte I2C payload: first byte is the register address,
  // second byte is the value to write into that register.
  uint8_t transmit_buffer[2] = {reg, val};


  if (HAL_I2C_Master_Transmit(hi2c, i2c_device_address, transmit_buffer, 2, 100) != HAL_OK) {
    return Ltr390Status::I2C_TRANSMIT_ERROR;
  }

  return Ltr390Status::OK;
}

Ltr390Status Ltr390Driver::read_reg8(uint8_t reg, uint8_t &val) const
{
  // Step 1: Send the register address to the sensor so it knows what to return.
  // Step 2: Read one byte back from that register.


  if (HAL_I2C_Master_Transmit(hi2c, i2c_device_address, &reg, 1, 100) != HAL_OK) {
    return Ltr390Status::I2C_TRANSMIT_ERROR;
  }
  if (HAL_I2C_Master_Receive(hi2c, i2c_device_address, &val, 1, 100) != HAL_OK) {
    return Ltr390Status::I2C_RECEIVE_ERROR;
  }

  return Ltr390Status::OK;
}

Ltr390Status Ltr390Driver::read_reg_n(uint8_t start_reg, uint8_t *buffer, uint16_t length) const
{
  // Step 1: Send the starting register address to the sensor.
  // Step 2: Read 'length' bytes back, starting from that register.
  // Used for UVS and ALS data, which are always 3 bytes each.


  if (HAL_I2C_Master_Transmit(hi2c, i2c_device_address, &start_reg, 1, 100) != HAL_OK) {
    return Ltr390Status::I2C_TRANSMIT_ERROR;
  }
  if (HAL_I2C_Master_Receive(hi2c, i2c_device_address, buffer, length, 100) != HAL_OK) {
    return Ltr390Status::I2C_RECEIVE_ERROR;
  }

  return Ltr390Status::OK;
}

Ltr390Status Ltr390Driver::set_mode(Mode mode) const
{
  /**
   * @brief Select ALS or UVS measurement mode (and ensure sensor enabled).
   *
   * Why: The LTR390 measures either ALS or UVS depending on MAIN_CTRL bits.
   * We always set ENABLE to ensure measurements are running.
   *
   * Note: Switching mode can require a short settling time before new data is ready.
   * We handle freshness by polling DATA_READY before reading UVS/ALS registers.
   */
	// Always set the ENABLE bit. Add the UVS bit only when switching to UV mode.
	  uint8_t control_register_value = MAIN_CTRL_ENABLE;

	  if (mode == Mode::ULTRAVIOLET) {
	    control_register_value |= MAIN_CTRL_UVS_MODE;
	  }

	  return write_reg8(REG_MAIN_CTRL, control_register_value);
}

Ltr390Status Ltr390Driver::wait_data_ready() const
{
  /**
   * @brief Poll the data-ready bit until a new sample is available or timeout occurs.
   *
   * Why polling (instead of interrupts) right now:
   * - bring-up is simpler and deterministic
   * - keeps ISRs short and avoids extra GPIO/EXTI complexity
   *
   * Constraint: DATA_READY_TIMEOUT_MS should be long enough for the chosen
   * integration time (resolution) + measurement rate.
   */
	const uint32_t start_tick = HAL_GetTick();

	  while ((HAL_GetTick() - start_tick) < DATA_READY_TIMEOUT_MS) {
	    uint8_t status_register = 0;

	    // Read the MAIN_STATUS register to check whether new data is available.
	    if (read_reg8(REG_MAIN_STATUS, status_register) != Ltr390Status::OK) {
	      return Ltr390Status::I2C_RECEIVE_ERROR;
	    }

	    // Bit 3 of MAIN_STATUS is set by the sensor when a fresh measurement is ready.
	    if ((status_register & MAIN_STATUS_DATA_READY) != 0u) {
	      return Ltr390Status::OK;
	    }

	    HAL_Delay(5);  // Short delay before polling again to avoid hammering the I2C bus.
	  }

	  return Ltr390Status::TIMEOUT;
}

Ltr390Status Ltr390Driver::begin()
{
  // Step 1: Check the sensor is reachable on I2C.
  if (probe() != Ltr390Status::OK) {
    return Ltr390Status::I2C_NO_ACKNOWLEDGMENT;
  }

  // Note: soft reset was removed because writing the reset bit causes the sensor
  // to briefly drop off I2C, returning a transmit error even though the reset
  // itself succeeds. The sensor powers up in a safe default state so the reset
  // is not needed.
  HAL_Delay(10);

  // Step 2: Set measurement resolution and rate.
  if (write_reg8(REG_MEAS_RATE, MEAS_RATE_DEFAULT) != Ltr390Status::OK) {
    return Ltr390Status::RESOLUTION_SETUP_ERROR;
  }

  // Step 3: Set gain to 18x.
  if (write_reg8(REG_GAIN, GAIN_18X) != Ltr390Status::OK) {
    return Ltr390Status::GAIN_SETUP_ERROR;
  }

  // Step 4: Enable sensor in UV mode.
  return set_mode(Mode::ULTRAVIOLET);
}

Ltr390Status Ltr390Driver::read_uvs_counts()
{
  // Switch the sensor to UV measurement mode.
  Ltr390Status status = set_mode(Mode::ULTRAVIOLET);
  if (status != Ltr390Status::OK) {
    return Ltr390Status::UV_MODE_SWITCH_ERROR;
  }

  // Wait until the sensor signals that a fresh UV measurement is available.
  status = wait_data_ready();
  if (status != Ltr390Status::OK) {
    return Ltr390Status::TIMEOUT;
  }

  // Read the 3 UV data bytes from registers 0x10, 0x11, 0x12.
  uint8_t raw_bytes[3] = {0, 0, 0};
  status = read_reg_n(REG_UVS_DATA0, raw_bytes, 3);
  if (status != Ltr390Status::OK) {
    return status;
  }

  // Combine the 3 bytes into a single 24-bit count and store in the member variable.
  uv_counts = pack_24bit_little_endian(raw_bytes);
  return Ltr390Status::OK;
}

Ltr390Status Ltr390Driver::read_als_counts()
{
  // Switch the sensor to ambient light measurement mode.
  Ltr390Status status = set_mode(Mode::AMBIENT_LIGHT);
  if (status != Ltr390Status::OK) {
    return Ltr390Status::ALS_MODE_SWITCH_ERROR;
  }

  // Wait until the sensor signals that a fresh ALS measurement is available.
  // ALS is useful for detecting shading or window obstruction alongside UV readings.
  status = wait_data_ready();
  if (status != Ltr390Status::OK) {
    return Ltr390Status::TIMEOUT;
  }

  // Read the 3 ALS data bytes from registers 0x0D, 0x0E, 0x0F.
  uint8_t raw_bytes[3] = {0, 0, 0};
  status = read_reg_n(REG_ALS_DATA0, raw_bytes, 3);
  if (status != Ltr390Status::OK) {
    return status;
  }

  // Combine the 3 bytes into a single 24-bit count and store in the member variable.
  als_counts = pack_24bit_little_endian(raw_bytes);
  return Ltr390Status::OK;
}

Ltr390Status Ltr390Driver::compute_uvi()
{
  // The window factor must be a positive number.
  // A value of 1.0 means no correction (no window, or perfectly transparent window).
  if (WINDOW_FACTOR <= 0.0f) {
    return Ltr390Status::INVALID_ARGUMENT;
  }



  // Divide the raw UV count by the reference sensitivity, then apply the window correction.
  uvi = (static_cast<float>(uv_counts) / COUNTS_PER_UVI) * WINDOW_FACTOR;
  return Ltr390Status::OK;
}
