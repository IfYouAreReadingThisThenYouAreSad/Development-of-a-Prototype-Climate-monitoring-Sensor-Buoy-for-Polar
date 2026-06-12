# GPS Module Driver (STM32)

# 1. About

This directory contains the driver used to interface with the **Adafruit Ultimate GPS module** for the buoy project.

The GPS module allows the system to monitor the **location of the buoy**, which may change rapidly due to ocean currents or drifting ice. The GPS also provides an accurate **UTC time reference**, which is used to synchronize the buoy's internal timers and periodically poll environmental sensors.

Additional information such as:

- Buoy speed
- Altitude
- Course direction
- Number of satellites in view

can also be obtained from the GPS module.

The GPS data is transmitted using **NMEA sentences over UART**, which are parsed and stored in a global data structure for use by the rest of the system.

---

# 2. Part Information

The GPS module used is the **Adafruit Ultimate GPS Module**, which can be purchased using the link below.

The module contains a **ceramic antenna** that works well in ideal conditions. However, for this project an **external GPS antenna** is used to improve signal reliability when the buoy is deployed in harsh environments.

To use the external antenna, a **uFL to SMA adapter** is required.

**Product Links**

+ Adafruit GPS Module: https://www.adafruit.com/product/746
+ GPS External Antenna: https://www.adafruit.com/product/960
+ Antenna Adapter: https://www.adafruit.com/product/851

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/GPS_Module/UltimateGPS.jpg"
    alt="Adafruit Ultimate GPS Module" width="400">
</p>

<p align="center">Figure 1: Adafruit Ultimate GPS Module</p>

---

# 3. Tested Hardware

This driver has been tested with:

- **STM32F401RCT6**
- **Adafruit Ultimate GPS Module**

---

# 4. Development Environment

The driver was developed using the following environment:

| Tool | Version |
|-----|--------|
| IDE | STM32CubeIDE |
| Version | 2.0.0 |
| Build | 26820_20251114_1348 |
| Vendor | STMicroelectronics |
| MCU | STM32F401RCT6 |
| Firmware Library | STM32 HAL |

---

# 5. Hardware Connections

The Adafruit Ultimate GPS module communicates with the STM32 using a **UART interface**.

The module outputs **NMEA sentences** which contain information such as time, position, speed, and satellite status.

## STM32 ↔ GPS Connections

| GPS Pin | STM32 Peripheral | Description |
|-------|------|-------------|
| VCC | 3.3V | Power supply |
| GND | GND | Ground |
| TX | UART_RX | GPS transmits NMEA data to STM32 |
| RX | UART_TX | STM32 sends configuration commands |
| EN | GPIO | Enables or disables the GPS module |

---

## Example STM32F401 Pin Mapping

| STM32 Pin | Peripheral | Function |
|-----------|-----------|----------|
| PB6 | UART1_TX | UART TX used to send commands |
| PB7 | UART1_RX | UART RX used to receive NMEA strings |
| PB9 | GPIO Output | Enable/disable GPS module |

---

# 6. API

## Functions

| Header | Function | Description |
|------|------|-------------|
| `gps.h` | `void gps_off(const GPS_HandleTypeDef *GPS_PINS)` | Turns off the GPS module |
| `gps.h` | `void gps_on(const GPS_HandleTypeDef *GPS_PINS)` | Turns on the GPS module |
| `gps.h` | `GPS_Status get_gps_data_timed(const GPS_HandleTypeDef *gps_pins)` | Attempts to acquire GPS data for a fixed duration using a timer (useful for cold starts in low power systems) |
| `gps.h` | `GPS_Status get_gps_data(const GPS_HandleTypeDef *gps_pins)` | Attempts to read GPS data once. Typically used when the GPS module is already running (warm start). |

---

# 7. Data Types (Structs)

## GPS_HandleTypeDef

| Type | Field | Description |
|-----|-----|-------------|
| `GPIO_TypeDef *` | `port` | GPIO port used for the enable pin |
| `uint16_t` | `pin` | GPIO pin used for the enable pin |
| `UART_HandleTypeDef *` | `uart` | UART interface connected to the GPS module |
| `TIM_HandleTypeDef *` | `tim` | Timer used by `get_gps_data_timed()` |

---

## GPS_DataTypeDef

When `gps.h` is included, a **global instance** of `GPS_DataTypeDef` called `GPS` is created.  
The GPS driver functions populate this structure with the latest parsed GPS data.

| Type | Field | Description |
|-----|-----|-------------|
| `GPS_Status` | `status` | Indicates whether GPS data acquisition was successful |
| `volatile uint8_t` | `buffer[100]` | Buffer storing the raw NMEA string |
| `volatile uint8_t` | `index` | Internal buffer index (not intended for user access) |
| `char *` | `token[10]` | Used internally for parsing the NMEA sentence |
| `char` | `time[10]` | Stores UTC time |
| `char` | `position[4][10]` | Stores latitude and longitude |
| `char` | `date[10]` | Stores date |
| `char` | `speed_knots[10]` | Speed in knots |
| `char` | `course[10]` | Course direction |
| `char` | `speed_kmh[10]` | Speed in kilometres per hour |
| `char` | `altitude[10]` | Altitude |

---

# 8. Macros

| Header | Macro | Description |
|------|------|-------------|
| `gps.h` | `DURATION` | Determines how many timer periods `get_gps_data_timed()` will run before stopping |

---

# 9. Enums

## GPS_Status

Used for debugging and to indicate the state of GPS data acquisition.

| Value | Description |
|------|-------------|
| `GPS_OK` | GPS command executed successfully |
| `GPS_POSITION_ACQUIRED` | GPS module obtained a valid satellite fix |
| `GPS_POSITION_NOT_ACQUIRED` | GPS module did not obtain a position fix |
| `GPS_OTHER_ACQUIRED` | Other data received but position not available |
| `GPS_ERROR_CHECKSUM` | Received NMEA sentence failed checksum validation |
| `GPS_ERROR_NMEA` | Error reading NMEA string from UART |
| `GPS_ERROR_BUFFER` | NMEA string exceeded buffer length |

---

# 10. Code Flow

For detailed information about individual functions, refer to the **header files**, which contain documentation describing parameters and return values.

An example implementation is provided in the **Example folder**, using an **STM32F401RCT6** with the **HAL driver library**.

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/GPS_Module/GPS%20Flow%20diagam.jpeg"
    alt="GPS processing flow diagram" width="400">
</p>

<p align="center">Figure 2: GPS data processing pipeline</p>

### Processing Pipeline

1. Turn on the GPS module  
2. Receive NMEA data via UART  
3. Perform checksum verification  
4. Parse the NMEA sentence  
5. Store decoded data in the global `GPS` structure  

**Note:** This process is encapsulated within the functions  
`get_gps_data_timed(..)` and `get_gps_data(..)`.

---

# 11. Dependencies

This driver requires:

- **STM32 HAL library**
- **UART peripheral**
- **Timer peripheral** (only required when using `get_gps_data_timed()`)

---

# 12. Example Usage

Example initialization and usage:

```c
GPS_HandleTypeDef gps_pins = {
    .port = GPIOB,
    .pin = GPIO_PIN_9,
    .uart = &huart1,
    .tim = &htim1
};

get_gps_data_timed(&gps_pins);
```

---

# 13. Notes

- The driver is designed for **low-power embedded systems**
- The GPS module may require several seconds to minutes to obtain a **cold start satellite fix**
- The `DURATION` macro in `gps.h` can be tuned depending on how long the system should wait for a GPS fix
- External antennas improve reliability in environments with poor satellite visibility
