/*
 * ltr390_driver.hpp
 *
 *  Created on: 3 Apr 2026
 *      Author: MuffadalTarawala
 *      Reviewer: ThomasNeedham
 *
 */




#pragma once

#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)
    #include "stm32h7xx_hal.h"
#elif defined(STM32F4) || defined(STM32F446xx) || defined(STM32F407xx) || defined(STM32F401xC) || defined(STM32F401xE)
    #include "stm32f4xx_hal.h"
#else
    #error "Unsupported STM32 family — check your -D compiler flags"
#endif
/**
 * @brief Status codes for LTR390 operations.
 */
enum class Ltr390Status : uint8_t {
  OK = 0,
  I2C_NO_ACKNOWLEDGMENT,
  I2C_TRANSMIT_ERROR,
  I2C_RECEIVE_ERROR,
  TIMEOUT,
  INVALID_ARGUMENT,
  RESOLUTION_SETUP_ERROR,
  GAIN_SETUP_ERROR,
  UV_MODE_SWITCH_ERROR,
  ALS_MODE_SWITCH_ERROR,
};

/**
 * @brief LTR390 driver (probe + read UVS/ALS counts).
 *
 * Notes/assumptions:
 * - I2C is configured + initialised by CubeMX (hi2c1).
 * - The Waveshare module handles level shifting/pull-ups.
 */
class Ltr390Driver {
public:
	enum class Mode : uint8_t {
	    AMBIENT_LIGHT = 0,
	    ULTRAVIOLET   = 1,
	  };

	Ltr390Driver(I2C_HandleTypeDef *hi2c_,
	               GPIO_TypeDef *sda_port_,
	               uint16_t sda_pin_,
	               GPIO_TypeDef *scl_port_,
	               uint16_t scl_pin_);

  /**
   * @brief Check if the device ACKs on I2C.
   * @return ok if ACKs, i2c_no_ack otherwise.
   */
  Ltr390Status probe() const;

  /**
   * @brief Apply a simple default configuration (gain/resolution/rate + enable).
   * Why: ensures stable measurements before reading data registers.
   */
  Ltr390Status begin();

  /**
   * @brief Read UVS raw counts (24-bit value).
   * @param uv_counts Output UV counts.
   */
  /**
     * @brief Read raw UVS counts from the sensor into uv_counts_ member.
     */
    Ltr390Status read_uvs_counts();

    /**
     * @brief Read raw ALS counts from the sensor into als_counts_ member.
     */
    Ltr390Status read_als_counts();

    /**
     * @brief Return the last stored UV counts (call read_uvs_counts first).
     */
    uint32_t get_uv_counts()  const { return uv_counts;  }

    /**
     * @brief Return the last stored ALS counts (call read_als_counts first).
     */
    uint32_t get_als_counts() const { return als_counts; }

    /**
     * @brief Return the last computed UVI value (call compute_uvi first).
     */
    float    get_uvi()        const { return uvi;        }

  /**
   * @brief Convert raw UVS counts into UV Index (UVI).
   *
   * This conversion assumes the reference settings:
   * - gain = 18x
   * - resolution = 20-bit
   * - integration time = 400 ms
   *
   * Compute UV Index using stored uv_counts_ and window_factor_.
   * Result is written into uvi_ member variable.
   * Call read_uvs_counts() first to update uv_counts_.
   */
  Ltr390Status compute_uvi();

private:
  // ----- Registers -----
  static constexpr uint8_t REG_MAIN_CTRL   = 0x00;
  static constexpr uint8_t REG_MEAS_RATE   = 0x04;
  static constexpr uint8_t REG_GAIN        = 0x05;
  static constexpr uint8_t REG_MAIN_STATUS = 0x07;
  static constexpr uint8_t REG_ALS_DATA0   = 0x0D;
  static constexpr uint8_t REG_UVS_DATA0   = 0x10;

  // ----- Bits -----
  static constexpr uint8_t MAIN_CTRL_SW_RESET = (1u << 4);
  static constexpr uint8_t MAIN_CTRL_UVS_MODE = (1u << 3);
  static constexpr uint8_t MAIN_CTRL_ENABLE   = (1u << 1);

  static constexpr uint8_t MAIN_STATUS_DATA_READY = (1u << 3);

  // Gain values: 18x = 0x04
  static constexpr uint8_t GAIN_18X = 0x04;

  // MEAS_RATE = (resolution bits) | (rate bits)
  // 20-bit / 400ms = 0x00, Rate 500ms = 0x04  => MEAS_RATE_DEFAULT = 0x04
  static constexpr uint8_t MEAS_RATE_DEFAULT = 0x04;

  static constexpr uint32_t DATA_READY_TIMEOUT_MS = 1000;

  I2C_HandleTypeDef *hi2c = nullptr;

  GPIO_TypeDef *sda_port = nullptr;
  uint16_t sda_pin = 0;

  GPIO_TypeDef *scl_port = nullptr;
  uint16_t scl_pin = 0;

  static constexpr uint8_t i2c_device_address = 0x53 << 1;

    // Sensor readings stored as member variables (point 6 + point 9)
    uint32_t uv_counts  = 0;
    uint32_t als_counts = 0;
    float    uvi        = 0.0f;

    // Window factor: compensates for UV loss through the buoy enclosure window.
    // Use 1.0f until the window material is calibrated (point 7).
    static constexpr float WINDOW_FACTOR = 1.0f;

    // Reference sensitivity constant for: 18x gain, 20-bit resolution, 400 ms integration.
    // This value comes from the LTR390 datasheet and is consistent with ESPHome's implementation.
    // If gain or resolution settings are changed, this constant must be recalculated.
    static constexpr float COUNTS_PER_UVI = 2300.0f;

  // Helpers (private so users can’t misuse low-level calls)
    // Low-level I2C helpers — private so application code cannot misuse them directly.
      Ltr390Status write_reg8(uint8_t reg, uint8_t val) const;
      Ltr390Status read_reg8(uint8_t reg, uint8_t &val) const;
      Ltr390Status read_reg_n(uint8_t start_reg, uint8_t *buffer, uint16_t length) const;

      Ltr390Status set_mode(Mode mode) const;
      Ltr390Status wait_data_ready() const;

      /**
       * @brief Pack 3 little-endian bytes from the sensor into one 24-bit value.
       * The LTR390 always returns UVS and ALS data as 3 bytes, LSB first.
       */
      static uint32_t pack_24bit_little_endian(const uint8_t bytes[3]);
};
