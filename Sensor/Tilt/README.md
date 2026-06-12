# Tilt Sensor Driver — IIS2ICLX (STM32)

# 1. About

This directory contains the driver for the **IIS2ICLX** dual-axis digital inclinometer used to monitor tilt.

Accurate tilt measurement is important for two reasons. First, it allows the ground station to detect whether the buoy has capsized or been displaced from a stable vertical orientation, which would affect the validity of other sensor readings. Second, tilt data can be used to compensate directional sensors (e.g. the EMI coil orientation) for angular offsets.

The driver:

- Configures the IIS2ICLX over SPI for the correct full-scale range and output data rate
- Reads raw X and Y axis acceleration values
- Applies an **exponential moving average (EMA)** filter to reduce noise
- Converts filtered acceleration values to **roll and pitch angles** in degrees

---

# 2. Part Information

| Field | Value |
|-------|-------|
| Manufacturer | STMicroelectronics |
| Part Number | IIS2ICLX |
| Type | 2-axis digital inclinometer |
| Full-scale range | ±0.5 g (CTRL1_XL = 0x1C) |
| Sensitivity | 0.061 mg/LSB |
| Interface | SPI |

**Datasheet:** https://www.st.com/en/mems-and-sensors/iis2iclx.html

> **Note:** The `FS_XL[1:0]` field in CTRL1_XL uses a non-standard encoding for the IIS2ICLX. A value of `0b11` selects **±0.5 g** at 0.061 mg/LSB, which is the opposite of the standard accelerometer convention. Do not change `CTRL1_XL` from `0x1C` without consulting the IIS2ICLX datasheet.

---

# 3. Tested Hardware

This driver has been tested with:

- **STM32F405RGT6** 
- **IIS2ICLX** 

---

# 4. Development Environment

| Tool | Version |
|------|---------|
| IDE | STM32CubeIDE |
| Version | 2.0.0 |
| Build | 26820_20251114_1348 |
| Vendor | STMicroelectronics |
| MCU | STM32F405RGT6 |
| Firmware Library | STM32 HAL |

---

# 5. Hardware Connections

The IIS2ICLX communicates with the STM32 over a 4-wire SPI interface.

## STM32 ↔ IIS2ICLX Connections

| IIS2ICLX Pin | STM32 Peripheral | Description |
|-------------|-----------------|-------------|
| VDD / VDDIO | 3.3 V | Power supply |
| GND | GND | Ground |
| SCL | SPI_SCK | SPI clock |
| SDA | SPI_MOSI | SPI data in |
| SDO | SPI_MISO | SPI data out |
| CS | GPIO Output | SPI chip select (active low) |

> **Note:** The SPI clock prescaler must be set to keep SCK within the IIS2ICLX maximum operating frequency.

---

# 6. API

## Functions

| Header | Function | Description |
|--------|----------|-------------|
| `tilt.h` | `void tilt_init(const Tilt_HandleTypeDef *tilt)` | Configures the IIS2ICLX over SPI and starts the EMA timer |
| `tilt.h` | `Tilt_Status tilt_read(const Tilt_HandleTypeDef *tilt, Tilt_DataTypeDef *data)` | Reads the latest EMA-filtered tilt angles |
| `tilt.h` | `void tilt_ema_update(void)` | Updates the EMA filter with a fresh ADC reading — called from the hardware timer ISR |

---

# 7. Data Types (Structs)

## Tilt_HandleTypeDef

| Type | Field | Description |
|------|-------|-------------|
| `SPI_HandleTypeDef *` | `spi` | SPI handle connected to the IIS2ICLX |
| `GPIO_TypeDef *` | `cs_port` | GPIO port for the SPI chip select pin |
| `uint16_t` | `cs_pin` | GPIO pin for the SPI chip select |
| `TIM_HandleTypeDef *` | `tim` | Timer handle used to periodically drive the EMA filter |

## Tilt_DataTypeDef

When `tilt.h` is included, a **global instance** of `Tilt_DataTypeDef` called `TILT` is created. The driver populates this structure after each call to `tilt_read()`.

| Type | Field | Description |
|------|-------|-------------|
| `Tilt_Status` | `status` | Indicates whether the read succeeded |
| `int16_t` | `raw_x` | Raw X-axis output register value |
| `int16_t` | `raw_y` | Raw Y-axis output register value |
| `float` | `ema_x` | EMA-filtered X-axis acceleration (g) |
| `float` | `ema_y` | EMA-filtered Y-axis acceleration (g) |
| `float` | `roll_deg` | Roll angle in degrees, derived from ema_x and ema_y |
| `float` | `pitch_deg` | Pitch angle in degrees, derived from ema_x and ema_y |

---

# 8. Macros

| Header | Macro | Description |
|--------|-------|-------------|
| `tilt.h` | `TILT_EMA_ALPHA` | EMA smoothing factor (0 < α ≤ 1); lower values give more smoothing |
| `tilt.h` | `TILT_SENSITIVITY` | Sensor sensitivity in g/LSB (0.061e-3 for CTRL1_XL = 0x1C) |
| `tilt.h` | `TILT_TIMER_PERIOD_MS` | EMA update interval in milliseconds (~80 ms) |

---

# 9. Enums

## Tilt_Status

| Value | Description |
|-------|-------------|
| `TILT_OK` | Read completed successfully |
| `TILT_ERROR_SPI` | SPI transaction failed or timed out |
| `TILT_ERROR_WHO_AM_I` | Device WHO_AM_I response did not match expected value |

---

# 10. Code Flow

### Initialisation

1. Assert chip select (low)
2. Write `CTRL1_XL = 0x1C` to configure ±0.5 g range and output data rate
3. Verify device identity by reading the WHO_AM_I register
4. Release chip select (high)
5. Start the hardware timer to trigger periodic EMA updates

### EMA Update (ISR, ~80 ms)

1. Assert chip select
2. Read X and Y axis output registers (two 16-bit signed values)
3. Convert raw values to acceleration in g using `TILT_SENSITIVITY`
4. Update EMA: `ema = α * new_sample + (1 − α) * ema`
5. Release chip select

### Read

1. Copy current `ema_x` and `ema_y` to output structure
2. Compute roll and pitch: `roll = atan2(ema_y, √(1 − ema_y²))`, `pitch = atan2(ema_x, √(1 − ema_x²))`
3. Store results in `Tilt_DataTypeDef`

---

# 11. Dependencies

- STM32 HAL library
- SPI peripheral
- GPIO (chip select)
- Hardware timer peripheral (EMA update clock)

---

# 12. Example Usage

```c
Tilt_HandleTypeDef tilt_handle = {
    .spi     = &hspi1,
    .cs_port = GPIOA,
    .cs_pin  = GPIO_PIN_4,
    .tim     = &htim6
};

tilt_init(&tilt_handle);

/* In the application loop — EMA is updated automatically by timer ISR */
tilt_read(&tilt_handle, &TILT);
```

---

# 13. Notes

- **The EMA filter must be driven continuously from a hardware timer.** 
- **Do not change `CTRL1_XL` from `0x1C`.** The IIS2ICLX uses a non-standard `FS_XL` encoding; changing to `0x10` will not select ±1 g as on a conventional accelerometer.
- Use a finite SPI timeout (e.g. 10 ms) in all `HAL_SPI_Transmit/Receive` calls. `HAL_MAX_DELAY` will hang indefinitely if the SPI peripheral is misconfigured.
