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
 * @brief Status codes returned by all LTR390 driver functions.
 *
 * Every public and private function returns one of these so the caller
 * can handle failures without inspecting HAL internals.
 */
enum class Ltr390Status : uint8_t {
  OK = 0,
  I2C_NO_ACKNOWLEDGMENT,   ///< Sensor did not ACK on I2C — check wiring/power
  I2C_TRANSMIT_ERROR,      ///< HAL failed to transmit a register address or value
  I2C_RECEIVE_ERROR,       ///< HAL failed to receive data from the sensor
  TIMEOUT,                 ///< Data-ready bit did not set within DATA_READY_TIMEOUT_MS
  INVALID_ARGUMENT,        ///< A guard condition failed (e.g. null handle, bad parameter)
  RESOLUTION_SETUP_ERROR,  ///< Writing MEAS_RATE register failed during init
  GAIN_SETUP_ERROR,        ///< Writing GAIN register failed during init
  UV_MODE_SWITCH_ERROR,    ///< Switching sensor to UV mode failed
  ALS_MODE_SWITCH_ERROR,   ///< Switching sensor to ALS mode failed
};

// ---------------------------------------------------------------------------
// These functions must be defined here in the header, above the class.
// They are called in constexpr member initialisers inside Ltr390Driver, so
// the compiler needs their full body visible at the point the header is included.
// Moving them to the .cpp would make constexpr evaluation impossible.
// ---------------------------------------------------------------------------

/**
 * @brief Derives the ALS integration time factor from the resolution field
 *        of MEAS_RATE_DEFAULT.
 *
 * Resolution field encoding (bits [6:4] of MEAS_RATE register):
 *   0 → 20-bit / 400ms → factor 4.0
 *   1 → 19-bit / 200ms → factor 2.0
 *   2 → 18-bit / 100ms → factor 1.0
 *   3 → 17-bit /  50ms → factor 0.5
 *   4 → 16-bit /  25ms → factor 0.25
 *
 * @param resolution_bits  Bits [6:4] extracted from MEAS_RATE_DEFAULT.
 * @return Integration time factor used in the lux calculation.
 */
static constexpr float resolve_integration_time(uint8_t resolution_bits)
{
    switch (resolution_bits)
    {
        case 0: return 4.0f;
        case 1: return 2.0f;
        case 2: return 1.0f;
        case 3: return 0.5f;
        case 4: return 0.25f;
        default: return 4.0f;  // Unknown value — fall back to slowest/most accurate
    }
}

/**
 * @brief Derives the numeric ALS gain multiplier from the GAIN register value.
 *
 * The GAIN register stores an encoded value, not the actual multiplier.
 * This function maps the register encoding to the real gain number used
 * in the lux formula (from the LTR390 datasheet, Table 5).
 *
 * Gain field encoding:
 *   0 →  1x gain
 *   1 →  3x gain
 *   2 →  6x gain
 *   3 →  9x gain
 *   4 → 18x gain
 *
 * @param gain_reg  Value of the GAIN register (e.g. GAIN = 0x04).
 * @return Numeric gain multiplier used in the lux calculation.
 */
static constexpr float resolve_gain(uint8_t gain_reg)
{
    switch (gain_reg)
    {
        case 0: return 1.0f;
        case 1: return 3.0f;
        case 2: return 6.0f;
        case 3: return 9.0f;
        case 4: return 18.0f;
        default: return 18.0f;  // Unknown value — fall back to 18x
    }
}

/**
 * @brief Driver for the LTR390 UV and ambient light sensor.
 *
 * Provides initialisation, raw count acquisition, and conversion to
 * UV Index and lux. All I2C communication goes through the STM32 HAL.
 *
 * Typical usage:
 *   Ltr390Driver sensor(&hi2c1);
 *   sensor.init();
 *   sensor.calculate_uv_index();   // reads UVS counts then converts
 *   float uvi = sensor.get_uv_index();
 *   sensor.calculate_lux();        // reads ALS counts then converts
 *   float lux = sensor.get_lux();
 *
 * Assumptions:
 *   - I2C peripheral is initialised by CubeMX before the driver is used.
 *   - The Waveshare breakout module handles level shifting and pull-up resistors.
 */
class Ltr390Driver {

  // -------------------------------------------------------------------------
  // Configuration constants — adjust these when changing sensor settings.
  // All other constants derive from these automatically at compile time.
  // -------------------------------------------------------------------------
private:

  /// Gain register value written to REG_GAIN. 0x04 = 18x gain.
  static constexpr uint8_t GAIN = 0x04;

  /**
   * @brief MEAS_RATE register value controlling resolution and measurement rate.
   *
   * Bits [6:4] set resolution / integration time.
   * Bits [2:0] set measurement rate.
   *
   * 0x04 = 0b00000100:
   *   bits [6:4] = 000 → 20-bit resolution, 400 ms integration time
   *   bits [2:0] = 100 → 500 ms measurement rate
   *
   * If you change this value, ALS_INTEGRATION_TIME updates automatically.
   */
  static constexpr uint8_t MEAS_RATE_DEFAULT = 0x04;

  /// Maximum time to wait for the DATA_READY bit before returning TIMEOUT.
  static constexpr uint32_t DATA_READY_TIMEOUT_MS = 1000;

  /**
   * @brief Correction factor for UV attenuation through the buoy enclosure window.
   *
   * UV light is heavily attenuated by acrylic and glass. Set to 1.0 (no
   * correction) until the window material has been characterised and calibrated.
   * A value greater than 1.0 amplifies the UVI reading to compensate for losses.
   */
  static constexpr float WINDOW_FACTOR = 1.0f;

  /**
   * @brief Sensitivity constant used to convert raw UVS counts to UV Index.
   *
   * Value sourced from the LTR390 datasheet and cross-referenced against
   * the ESPHome implementation. Valid only for the reference settings:
   *   - Gain:             18x
   *   - Resolution:       20-bit
   *   - Integration time: 400 ms
   *
   * If GAIN or MEAS_RATE_DEFAULT are changed, this constant must be
   * recalculated from the datasheet.
   */
  static constexpr float COUNTS_PER_UVI = 2300.0f;

  /// Fixed ALS sensitivity constant from the LTR390 datasheet.
  static constexpr float ALS_SENSITIVITY = 0.6f;



public:

  /// Sensor measurement mode — selects which photodiode result is active.
  enum class Mode : uint8_t {
    AMBIENT_LIGHT = 0,  ///< Visible light measurement (ALS)
    ULTRAVIOLET   = 1,  ///< UV measurement (UVS)
  };

  /**
   * @brief Construct the driver with an I2C handle.
   *
   * The I2C peripheral must already be initialised by CubeMX.
   * GPIO pin/port parameters are not required — the HAL handle
   * already encapsulates the hardware configuration.
   *
   * @param hi2c_  Pointer to the initialised I2C handle (e.g. &hi2c1).
   */
  Ltr390Driver(I2C_HandleTypeDef *hi2c_);

  /**
   * @brief Initialise the sensor with default configuration.
   *
   * Performs the following steps:
   *   1. Probes the I2C bus to confirm the sensor is present.
   *   2. Writes the measurement rate / resolution register.
   *   3. Writes the gain register.
   *   4. Enables the sensor in UV measurement mode.
   *
   * Call this once after construction before reading any measurements.
   *
   * @return OK on success, or a specific error code on failure.
   */
  Ltr390Status init();

  /**
   * @brief Read raw UVS counts and convert them to UV Index.
   *
   * Switches the sensor to UV mode, waits for a fresh measurement,
   * reads the raw 24-bit count, then applies the datasheet formula:
   *
   *   UVI = (uv_counts / COUNTS_PER_UVI) * WINDOW_FACTOR
   *
   * The result is stored internally and retrieved with get_uv_index().
   *
   * @return OK on success, or a specific error code on failure.
   */
  Ltr390Status calculate_uv_index();

  /**
   * @brief Read raw ALS counts and convert them to lux.
   *
   * Switches the sensor to ALS mode, waits for a fresh measurement,
   * reads the raw 24-bit count, then applies the datasheet formula:
   *
   *   Lux = (ALS_SENSITIVITY * als_counts) / (ALS_GAIN * ALS_INTEGRATION_TIME)
   *
   * The result is stored internally and retrieved with get_lux().
   *
   * @return OK on success, or a specific error code on failure.
   */
  Ltr390Status calculate_lux();

  /**
   * @brief Return the last lux value computed by calculate_lux().
   *
   * Call calculate_lux() first to update this value.
   */
  float get_lux() const { return lux; }

  /**
   * @brief Return the last UV Index value computed by calculate_uv_index().
   *
   * Call calculate_uv_index() first to update this value.
   */
  float get_uv_index() const { return uv_index; }

private:

  // -------------------------------------------------------------------------
  // Register addresses (from LTR390 datasheet, Table 4)
  // -------------------------------------------------------------------------
  static constexpr uint8_t REG_MAIN_CTRL   = 0x00;  ///< Mode select and enable
  static constexpr uint8_t REG_MEAS_RATE   = 0x04;  ///< Resolution and measurement rate
  static constexpr uint8_t REG_GAIN        = 0x05;  ///< Gain setting
  static constexpr uint8_t REG_MAIN_STATUS = 0x07;  ///< Data-ready and interrupt flags
  static constexpr uint8_t REG_ALS_DATA0   = 0x0D;  ///< ALS data LSB (3 bytes: 0x0D–0x0F)
  static constexpr uint8_t REG_UVS_DATA0   = 0x10;  ///< UVS data LSB (3 bytes: 0x10–0x12)

  // -------------------------------------------------------------------------
  // MAIN_CTRL register bit masks
  // -------------------------------------------------------------------------
  static constexpr uint8_t MAIN_CTRL_SW_RESET = (1u << 4);  ///< Software reset
  static constexpr uint8_t MAIN_CTRL_UVS_MODE = (1u << 3);  ///< 1 = UVS mode, 0 = ALS mode
  static constexpr uint8_t MAIN_CTRL_ENABLE   = (1u << 1);  ///< Enable measurements

  /// Bit 3 of MAIN_STATUS — set by sensor when a fresh measurement is ready.
  static constexpr uint8_t MAIN_STATUS_DATA_READY = (1u << 3);

  /// I2C address pre-shifted for the STM32 HAL (7-bit address 0x53, left-shifted by 1).
  static constexpr uint8_t i2c_device_address = 0x53 << 1;

  // -------------------------------------------------------------------------
  // Compile-time integration time derivation.
  // These three constants must remain in this order — each depends on the one above.
  // -------------------------------------------------------------------------

  /// Resolution field extracted from bits [6:4] of MEAS_RATE_DEFAULT.
  static constexpr uint8_t MEAS_RATE_RESOLUTION_BITS = (MEAS_RATE_DEFAULT >> 4) & 0x07;

  /**
   * @brief Integration time factor derived automatically from MEAS_RATE_DEFAULT.
   *
   * Updated at compile time whenever MEAS_RATE_DEFAULT changes — no manual
   * adjustment needed.
   */
  static constexpr float ALS_INTEGRATION_TIME = resolve_integration_time(MEAS_RATE_RESOLUTION_BITS);

  /**
   * @brief Numeric ALS gain multiplier derived automatically from the GAIN register value.
   *
   * Resolved at compile time via resolve_gain() — if GAIN is changed,
   * this updates automatically with no manual adjustment needed.
   */
  static constexpr float ALS_GAIN = resolve_gain(GAIN);




  // -------------------------------------------------------------------------
  // Member variables
  // -------------------------------------------------------------------------

  /// HAL I2C handle — provided at construction, never null after a successful init().
  I2C_HandleTypeDef *hi2c = nullptr;

  uint32_t uv_counts  = 0;      ///< Last raw UVS count read from the sensor
  uint32_t als_counts = 0;      ///< Last raw ALS count read from the sensor
  float    uv_index   = 0.0f;   ///< Last UV Index computed by calculate_uv_index()
  float    lux        = 0.0f;   ///< Last lux value computed by calculate_lux()

  // -------------------------------------------------------------------------
  // Private helpers — not part of the public API
  // -------------------------------------------------------------------------

  /**
   * @brief Check that the sensor ACKs on I2C.
   *
   * Used internally by init() to catch wiring or power problems early.
   * Returns OK if the sensor responds, I2C_NO_ACKNOWLEDGMENT otherwise.
   */
  Ltr390Status probe() const;

  /**
   * @brief Read raw UVS counts into uv_counts.
   *
   * Switches to UV mode, waits for data ready, then reads 3 bytes from
   * REG_UVS_DATA0 and packs them into a 24-bit value.
   */
  Ltr390Status read_uvs_counts();

  /**
   * @brief Read raw ALS counts into als_counts.
   *
   * Switches to ALS mode, waits for data ready, then reads 3 bytes from
   * REG_ALS_DATA0 and packs them into a 24-bit value.
   */
  Ltr390Status read_als_counts();

  /// Write a single byte to a sensor register.
  Ltr390Status write_reg8(uint8_t reg, uint8_t val) const;

  /// Read a single byte from a sensor register.
  Ltr390Status read_reg8(uint8_t reg, uint8_t &val) const;

  /// Read multiple consecutive bytes starting from start_reg.
  Ltr390Status read_reg_n(uint8_t start_reg, uint8_t *buffer, uint16_t length) const;

  /// Set the sensor measurement mode (ALS or UVS) and ensure it is enabled.
  Ltr390Status set_mode(Mode mode) const;

  /**
   * @brief Poll MAIN_STATUS until DATA_READY is set or the timeout expires.
   *
   * Polling is used in place of interrupts to keep bring-up simple and
   * avoid EXTI/ISR complexity. DATA_READY_TIMEOUT_MS must be long enough
   * to cover the configured integration time plus measurement rate.
   */
  Ltr390Status wait_data_ready() const;

  /**
   * @brief Pack 3 little-endian bytes from the sensor into a 24-bit value.
   *
   * The LTR390 returns UVS and ALS data as 3 bytes, LSB first (bytes 0, 1, 2
   * map to bits 7:0, 15:8, 23:16 respectively).
   *
   * @param bytes  Pointer to exactly 3 bytes in little-endian order.
   * @return 24-bit count stored in the lower 24 bits of a uint32_t.
   */
  static uint32_t pack_24bit_little_endian(const uint8_t bytes[3]);
};
