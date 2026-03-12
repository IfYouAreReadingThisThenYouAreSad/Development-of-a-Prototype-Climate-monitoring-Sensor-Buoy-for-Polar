 LCD Module Driver (STM32)

# 1. About

This directory contains the driver used to interface with the **Waveshare LCD1602 I2C Module V2.0** for the buoy project.

The GPS module allows the system to monitor the **location of the buoy**, which may change rapidly due to ocean currents or drifting ice. The GPS also provides an accurate **UTC time reference**, which is used to synchronize the buoy's internal timers and periodically poll environmental sensors.
The LCD module allows for system verification of each subsystem of the buoy on start up to imediately idicate if there is a problem which would be vital on deployment of the protype. adititonally the LCD module is used for **Debuggin** which will be vital for intergration of every subsystem , espically 
thoes that cannot be stepped through with the STM32IDE debugger tool such as the camera capture function.


---

# 2. Part Information

The LCD module used is the **Waveshare Blue 16x2 I2C LCD Module - 3.3V/5V with Backlight Controler**, which can be purchased using the link below.

**Product Links**

+ Waveshare Blue 16x2 I2C LCD Module - 3.3V/5V with Backlight Controle: https://thepihut.com/products/waveshare-blue-16x2-i2c-lcd-module-3-3v-5v-with-backlight-control


<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/LCD_Module/LCD_Module_image.webp"
    alt="Adafruit Ultimate GPS Module" width="400">
</p>

<p align="center">Figure 1: Waveshare LCD Module</p>

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
| SCL | I2C_SCL | I2C clock used to configure AiP31068L |
| SDA | I2C_SDA | I2C data line used for AiP31068L configuration |

---

## Example STM32F401 Pin Mapping

| STM32 Pin | Peripheral | Function |
|-----------|-----------|----------|
| PB6 | I2C1_SCL | I2C clock |
| PB7 | I2C1_SDA | I2C data |

---

# 6. API

## Functions

| Function                                          | Description                                                                |
| ------------------------------------------------- | -------------------------------------------------------------------------- |
| `LCD_I2C_WRITE_COMMAND(uint8_t function_byte)`    | Sends a command byte to the LCD via I2C.                                   |
| `LCD_I2C_WRITE_DATA(uint8_t function_byte)`       | Sends a data byte (character) to the LCD via I2C.                          |
| `I2C_WRITE_BACKLIGHT(uint8_t reg, uint8_t val)`   | Writes a value to a backlight controller register via I2C.                 |
| `BACKLIGHT_INIT(void)`                            | Initializes the SN3193 backlight controller and sets default LED settings. |
| `SET_BRIGHTNESS(uint8_t value)`                   | Sets backlight brightness (0–100%) using PWM.                              |
| `SET_LED_MODE(uint8_t mode)`                      | Sets LED operation mode: steady (`0x00`) or breathing (`0x20`).            |
| `DISPLAY(void)`                                   | Turns on the LCD display.                                                  |
| `CLEAR(void)`                                     | Clears the LCD display and resets the cursor.                              |
| `SET_CURSOR(uint8_t col, uint8_t row)`            | Moves the cursor to a specific column and row.                             |
| `DATA(uint8_t value)`                             | Sends a single character to the LCD display.                               |
| `SEND_STRING(const char *str)`                    | Sends a string to the LCD display.                                         |
| `NOCURSOR(void)`                                  | Hides the cursor on the display.                                           |
| `CURSOR(void)`                                    | Shows the cursor on the display.                                           |
| `SCROLL_DISPLAY_LEFT(void)`                       | Scrolls the display content to the left.                                   |
| `SCROLL_DISPLAY_RIGHT(void)`                      | Scrolls the display content to the right.                                  |
| `LEFT_TO_RIGHT(void)`                             | Sets text direction to left-to-right.                                      |
| `RIGHT_TO_LEFT(void)`                             | Sets text direction to right-to-left.                                      |
| `NOAUTOSCROLL(void)`                              | Disables automatic text scrolling.                                         |
| `AUTOSCROLL(void)`                                | Enables automatic text scrolling.                                          |
| `CREATECHAR(uint8_t location, uint8_t charmap[])` | Creates a custom character at a CGRAM location (0–7).                      |
| `LCD_INIT(void)`                                  | Initializes the LCD display, configures lines, cursor, and entry mode.     |

---


# 8. Macros

| Header | Macro | Description |
|--------|-------|-------------|
| `WAVESHARE_LCD.H` | `LCD_ADDRESS` | I2C address of LCD (`0x3E << 1`) |
| `WAVESHARE_LCD.H` | `LCD_LIGHT_ADDRESS` | I2C address of LCD backlight (`0x6B << 1`) |

<!-- LCD Command Set -->
| `WAVESHARE_LCD.H` | `LCD_CLEARDISPLAY` | Command to clear the display |
| `WAVESHARE_LCD.H` | `LCD_RETURNHOME` | Command to return cursor to home position |
| `WAVESHARE_LCD.H` | `LCD_ENTRYMODESET` | Command to set entry mode |
| `WAVESHARE_LCD.H` | `LCD_DISPLAYCONTROL` | Command to control display settings |
| `WAVESHARE_LCD.H` | `LCD_CURSORSHIFT` | Command to shift cursor or display |
| `WAVESHARE_LCD.H` | `LCD_FUNCTIONSET` | Command to set function (like display mode) |
| `WAVESHARE_LCD.H` | `LCD_SETCGRAMADDR` | Command to set CGRAM address |
| `WAVESHARE_LCD.H` | `LCD_SETDDRAMADDR` | Command to set DDRAM address |

<!-- Entry Mode Flags -->
| `WAVESHARE_LCD.H` | `LCD_ENTRYRIGHT` | Cursor moves right |
| `WAVESHARE_LCD.H` | `LCD_ENTRYLEFT` | Cursor moves left |
| `WAVESHARE_LCD.H` | `LCD_ENTRYSHIFTINCREMENT` | Shift display to the right automatically |
| `WAVESHARE_LCD.H` | `LCD_ENTRYSHIFTDECREMENT` | Shift display to the left automatically |

<!-- Display Control Flags -->
| `WAVESHARE_LCD.H` | `LCD_DISPLAYON` | Turn the display on |
| `WAVESHARE_LCD.H` | `LCD_DISPLAYOFF` | Turn the display off |
| `WAVESHARE_LCD.H` | `LCD_CURSORON` | Show the cursor |
| `WAVESHARE_LCD.H` | `LCD_CURSOROFF` | Hide the cursor |
| `WAVESHARE_LCD.H` | `LCD_BLINKON` | Blink the cursor |
| `WAVESHARE_LCD.H` | `LCD_BLINKOFF` | Do not blink the cursor |

<!-- Cursor and Display Shift Flags -->
| `WAVESHARE_LCD.H` | `LCD_DISPLAYMOVE` | Move the display |
| `WAVESHARE_LCD.H` | `LCD_CURSORMOVE` | Move the cursor |
| `WAVESHARE_LCD.H` | `LCD_MOVERIGHT` | Move display/cursor to the right |
| `WAVESHARE_LCD.H` | `LCD_MOVELEFT` | Move display/cursor to the left |

<!-- Function Set Flags -->
| `WAVESHARE_LCD.H` | `LCD_8BITMODE` | Set 8-bit mode |
| `WAVESHARE_LCD.H` | `LCD_4BITMODE` | Set 4-bit mode |
| `WAVESHARE_LCD.H` | `LCD_2LINE` | Use 2 lines |
| `WAVESHARE_LCD.H` | `LCD_1LINE` | Use 1 line |
| `WAVESHARE_LCD.H` | `LCD_5X8DOTS` | Set 5x8 dots for characters |

<!-- SN3193 LED Controller -->
| `WAVESHARE_LCD.H` | `SN3193_IIC_ADDRESS` | I2C address of SN3193 backlight controller |
| `WAVESHARE_LCD.H` | `SHUTDOWN_REG` | Software shutdown register |
| `WAVESHARE_LCD.H` | `BREATHING_CONTROL_REG` | Breathing effect control register |
| `WAVESHARE_LCD.H` | `LED_MODE_REG` | LED mode control register |
| `WAVESHARE_LCD.H` | `LED_NORMAL_MODE` | Normal LED operation |
| `WAVESHARE_LCD.H` | `LED_BREATH_MODE` | Breathing LED operation |
| `WAVESHARE_LCD.H` | `CURRENT_SETTING_REG` | Set output current |
| `WAVESHARE_LCD.H` | `PWM_1_REG` | PWM duty cycle for channel 1 |
| `WAVESHARE_LCD.H` | `PWM_2_REG` | PWM duty cycle for channel 2 |
| `WAVESHARE_LCD.H` | `PWM_3_REG` | PWM duty cycle for channel 3 |
| `WAVESHARE_LCD.H` | `PWM_UPDATE_REG` | Load PWM settings |
| `WAVESHARE_LCD.H` | `T0_1_REG` | Set T0 time for OUT1 |
| `WAVESHARE_LCD.H` | `T0_2_REG` | Set T0 time for OUT2 |
| `WAVESHARE_LCD.H` | `T0_3_REG` | Set T0 time for OUT3 |
| `WAVESHARE_LCD.H` | `T1T2_1_REG` | Set T1&T2 time for OUT1 |
| `WAVESHARE_LCD.H` | `T1T2_2_REG` | Set T1&T2 time for OUT2 |
| `WAVESHARE_LCD.H` | `T1T2_3_REG` | Set T1&T2 time for OUT3 |
| `WAVESHARE_LCD.H` | `T3T4_1_REG` | Set T3&T4 time for OUT1 |
| `WAVESHARE_LCD.H` | `T3T4_2_REG` | Set T3&T4 time for OUT2 |
| `WAVESHARE_LCD.H` | `T3T4_3_REG` | Set T3&T4 time for OUT3 |
| `WAVESHARE_LCD.H` | `TIME_UPDATE_REG` | Load time registers |
| `WAVESHARE_LCD.H` | `LED_CONTROL_REG` | Enable LED outputs OUT1~OUT3 |
| `WAVESHARE_LCD.H` | `RESET_REG` | Reset all registers to default values |




