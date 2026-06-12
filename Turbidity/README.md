# Turbidity Sensor Driver (STM32)

# 1. About

This directory contains the driver for the **custom optical turbidity sensor** used to measure water clarity.

Turbidity is a measure of the cloudiness or haziness of water caused by suspended particulates such as sediment, algae, or ice crystals. In polar environments, turbidity is an important indicator of glacial melt input, biological activity, and sediment transport.

The sensor uses a transmission-based optical design:

- An **infrared LED** emits light through the water column
- A **photodiode** on the opposing side detects the transmitted intensity
- Higher turbidity causes greater scattering and absorption, reducing the received signal
- The photodiode current is converted to a voltage and sampled by the STM32 ADC

The driver reads the ADC value, converts it to a normalised turbidity reading in Nephelometric Turbidity Units (NTU).

---

# 2. Part Information

The turbidity sensor is a custom design using discrete components. There is no external turbidity IC.

| Component | Description |
|-----------|-------------|
| IR LED | Infrared emitter (850 nm typical) |
| Photodiode | Infrared photodiode, matched to LED wavelength |
| Signal conditioning | Transimpedance amplifier or load resistor for photodiode current-to-voltage conversion |
| Measurement | STM32 ADC |

---

# 3. Tested Hardware

This driver has been tested with:

- **STM32F405RGT6**

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

The turbidity sensor consists of an IR LED driven from a GPIO pin and a photodiode whose output is sampled by the STM32 ADC.

## STM32 ↔ Turbidity Sensor Connections

| Signal | STM32 Peripheral | Description |
|--------|-----------------|-------------|
| IR LED drive | GPIO Output | Enables the IR LED transmitter |
| Photodiode output | ADC input | Analog channel |

---

# 6. API

## Functions

| Header | Function | Description |
|--------|----------|-------------|
| `turbidity.h` | `void turbidity_init(const Turbidity_HandleTypeDef *turb)` | Configures GPIO and ADC for turbidity measurement |
| `turbidity.h` | `Turbidity_Status turbidity_read(const Turbidity_HandleTypeDef *turb, Turbidity_DataTypeDef *data)` | Enables LED, performs ADC measurement, converts to NTU, disables LED |

---

# 7. Data Types (Structs)

## Turbidity_HandleTypeDef

| Type | Field | Description |
|------|-------|-------------|
| `ADC_HandleTypeDef *` | `adc` | ADC handle used to sample the photodiode output |
| `uint32_t` | `adc_channel` | ADC channel connected to the photodiode signal |
| `GPIO_TypeDef *` | `led_port` | GPIO port for the IR LED drive pin |
| `uint16_t` | `led_pin` | GPIO pin for the IR LED drive |

## Turbidity_DataTypeDef

When `turbidity.h` is included, a **global instance** of `Turbidity_DataTypeDef` called `TURBIDITY` is created. The driver populates this structure after each call to `turbidity_read()`.

| Type | Field | Description |
|------|-------|-------------|
| `Turbidity_Status` | `status` | Indicates whether the measurement succeeded |
| `uint16_t` | `adc_raw` | Raw 12-bit ADC value from the photodiode |
| `float` | `voltage` | ADC voltage in volts |
| `float` | `ntu` | Estimated turbidity in Nephelometric Turbidity Units |

---

# 8. Macros

| Header | Macro | Description |
|--------|-------|-------------|
| `turbidity.h` | `TURBIDITY_ADC_SAMPLES` | Number of ADC samples averaged per measurement |
| `turbidity.h` | `TURBIDITY_ADC_REF_V` | ADC reference voltage in volts (matches system ADC reference) |
| `turbidity.h` | `TURBIDITY_LED_SETTLE_MS` | Delay in milliseconds between LED enable and ADC sample (allows LED to stabilise) |
| `turbidity.h` | `TURBIDITY_NTU_SCALE` | Empirical scale factor for voltage-to-NTU conversion |
| `turbidity.h` | `TURBIDITY_NTU_OFFSET` | Empirical offset for voltage-to-NTU conversion |

---

# 9. Enums

## Turbidity_Status

| Value | Description |
|-------|-------------|
| `TURBIDITY_OK` | Measurement completed successfully |
| `TURBIDITY_ERROR_ADC` | ADC conversion failed or timed out |

---

# 10. Code Flow

1. Read ambient voltage
2. Enable IR LED GPIO pin (LED on)
3. Read excited voltage
4. Disable IR LED GPIO pin (LED off)
5. Calculate backscatter voltage
6. Convert raw ADC count to voltage
7. Convert voltage to NTU 

---

# 11. Dependencies

- STM32 HAL library
- ADC peripheral
- GPIO (IR LED drive)

---

# 12. Example Usage

```c
Turbidity_HandleTypeDef turb_handle = {
    .adc         = &hadc1,
    .adc_channel = ADC_CHANNEL_5,
    .led_port    = GPIOC,
    .led_pin     = GPIO_PIN_0
};

turbidity_init(&turb_handle);
turbidity_read(&turb_handle, &TURBIDITY);
```

---

# 13. Notes

- **Calibration is required.** The NTU conversion (`TURBIDITY_NTU_SCALE` and `TURBIDITY_NTU_OFFSET`) is determined empirically using water samples of known turbidity calibrated with kaolin clay. The relationship between transmitted intensity and NTU depends on the LED wavelength, optical path length, and conditioning circuit gain.
- The IR LED is enabled only during measurement to reduce average power consumption and extend LED lifetime. The settle delay `TURBIDITY_LED_SETTLE_MS` must be long enough for the LED output to reach a stable intensity before sampling.
- Use a finite ADC timeout in all HAL calls. `HAL_MAX_DELAY` will hang indefinitely on a misconfigured peripheral.
