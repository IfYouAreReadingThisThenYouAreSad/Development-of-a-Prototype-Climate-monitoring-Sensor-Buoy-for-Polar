# External Temperature (STM32)

---

# 1. Overview

A custom made external temperature sensor was made for the buoy that allowed 360 degree external temperature monitoring, while allowing the buoy to be sealant and waterproof.

The sensor itself used a PS103J2 thermistor. This was then placed inside a hollowed out coach bolt. The bolt was then filled with thermal paste to maximise thermal transfer between the bolt and the thermistor, and the thermistor was then fixed in place with resin and heatshrink. A diagram of this is shown below.

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/External%20Temperature/2D%20View%20External%20Temperature%20Sensor.png" width="400">
</p>

<p align="center"><b>Figure 1:</b> 2D View of Custom Made External Temperature Sensor.</p>

The thermistors were connected to small JST connectors (1.25mm). These small connectors allowed a nut to easily pass each temperature sensor, allowing it to be secured to the buoy. Each sensor (4 in total) then plugged into a custom made Donut Temperature PCB shown below. This allowed all of them to combine onto one header, so one wire could go from the Donut PCB to the daughterboard to read all the external temperature sensors. This helped reduce wiring in the buoy and supported the buoy design philosophy of allowing every external temperature sensor to be modular and easy to assemble.

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/External%20Temperature/Temperature%20Donut.JPG" width="400">
</p>

<p align="center"><b>Figure 2:</b> Custom made Temperature PCB</p>

3.3V from the Donut PCB goes into the thermistors. Each thermistor then connects to the simplified circuit below, which is on the daughter PCB. 3.3V goes into the thermistor, which forms a potential divider with a 15kΩ resistor and 100nF capacitor; this is then fed into an ADC channel on the STM32 as shown in the simplified diagram below.

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/External%20Temperature/temp_filter_no_buffer.png" width="400">
</p>

<p align="center"><b>Figure 3:</b> Simplified Thermistor Circuit.</p>

---

# 2. Hardware

The system uses the:

**PS103J2 (NTC Thermistor)**

### Product Link
https://www.digikey.co.uk/en/products/detail/littelfuse-inc/ps103j2/1014531

### Datasheet
https://www.littelfuse.com/assetdocs/littelfuse-leaded-thermistors-standard-precision-ps-datasheet?assetguid=f2c5cde0-806d-4632-bdd3-5e56183e2fd4

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/External%20Temperature/PS104J2_sml.jpg" width="400">
</p>

<p align="center"><b>Figure 4:</b> PS103J2 NTC Thermistor</p>

---

# 3. CubeMX Setup

As there are 4 analogue external temperature sensors and only 3 ADC channels available on the STM32F405RGT6 (one used for testing), and each sensor needs to be read quickly, some alternative setup in CubeMX is required. If your STM32 has more channels available then you can set up the thermistors normally.

For this project, one ADC channel was used to get around the channel shortage. Each instance of this channel was assigned a rank, set to 480 cycles, and put into scan conversion mode. In the code, ADC reconfiguration is used in the functions to reset the ADC before reading it — this is also why there are a lot of small delays in the code.

A screenshot of how 2 instances of the same ADC channel are set up is shown below. Note this image was taken before the code and system were all switched to 1 ADC channel with 4 instances, but you just keep adding ranks in the settings in the same way.

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/External%20Temperature/CubeMx%20ADC%20Set%20Up.jpeg" width="400">
</p>

<p align="center"><b>Figure 5:</b> ADC Setup</p>

---

# 4. Hardware Connections

The thermistors are read via the **STM32 ADC** (analogue input), not I2C.

## STM32 ↔ Thermistor Connections

| Thermistor Circuit Pin | STM32 / System | Description                          |
|-------------------------|-----------------|---------------------------------------|
| 3.3V                    | 3.3V            | Supply to the potential divider        |
| Divider output          | ADC1_INx        | Analogue voltage proportional to temp  |
| GND                     | GND             | Common ground                          |

## STM32F405RGT6 Pin Mapping

| STM32 Pin | Peripheral | Function                  |
|-----------|------------|---------------------------|
| PC0       | ADC1_IN10  | Thermistor channel 1       |
| PC1       | ADC1_IN11  | Thermistor channel 2       |
| PC2       | ADC1_IN12  | Thermistor channel 3       |
| PC3       | ADC1_IN13  | Thermistor channel 4       |

**Note:** PC0 is configured as analogue by default via `MX_ADC1_Init()`. PC1, PC2, and PC3 must also be set to `GPIO_MODE_ANALOG` in the `.ioc` file for all four channels to be usable.

---

# 5. API

## Initialisation

| Function | Description |
|----------|-------------|
| `void temp_sensor_init(void)` | Overrides the `MX_ADC1_Init()` scan configuration to single-conversion mode, which is required for per-channel dynamic switching in `temp_sensor_read()`. Must be called after `MX_ADC1_Init()` and before `sensors_init()`. |

---

## Important `#define` Constants

| Variable | Description |
|----------|-------------|
| `TEMPERATURE_ERROR_SENTINEL` | Value (`-99.9f`) returned for any channel that fails to produce a valid reading. |

---

## Public Functions

| Function | Description |
|----------|-------------|
| `void temp_sensor_init(void)` | Reconfigures ADC1 into single-conversion mode for per-channel reads. |
| `void temp_sensor_read(uint8_t *out, uint8_t *len_out)` | Reads all four thermistor channels sequentially and writes 4 × float32 temperatures (°C) into `out`. `out` must be at least 16 bytes. `len_out` is set to 16 and may be `NULL`. |

---

# 6. Code Flow

## Processing Steps

1. **Initialisation (run once)**
   - Reconfigures ADC1 from scan mode to single-conversion mode
   - Prepares the driver for sequential per-channel reads

2. **Measurement**
   - Reconfigures and triggers ADC1 for each thermistor channel (PC0–PC3 / ADC1_IN10–IN13) in turn
   - Small delays are used between reconfigurations to allow the ADC to settle
   - Reads the raw ADC value for each channel

3. **Conversion & Output**
   - Converts each channel's raw ADC reading to a temperature in °C using the thermistor's resistance/temperature relationship
   - Any channel that fails to produce a valid reading is set to `TEMPERATURE_ERROR_SENTINEL` (-99.9°C)
   - Writes all 4 channel temperatures as float32 values into the output buffer (16 bytes total)

---

# 7. Dependencies

- STM32 HAL Library
- STM32 ADC1 Peripheral

---

# 8. Example Usage (C)

```c
uint8_t  ext_temp_buf[16];
uint8_t  ext_temp_len = 0;

temp_sensor_init();   // call after MX_ADC1_Init(), before sensors_init()

while (1)
{
    temp_sensor_read(ext_temp_buf, &ext_temp_len);

    float ch1, ch2, ch3, ch4;
    memcpy(&ch1, &ext_temp_buf[0],  4);
    memcpy(&ch2, &ext_temp_buf[4],  4);
    memcpy(&ch3, &ext_temp_buf[8],  4);
    memcpy(&ch4, &ext_temp_buf[12], 4);

    BSP_LED_Toggle(LED_GREEN);
    HAL_Delay(500);
}
```
