# LCD Module Driver (STM32)

This library is based heavily on the **Waveshare Arduino LCD library**, but includes several modifications to make it more user-friendly and easier to integrate with STM32 projects.

If you are looking for libraries for **Arduino, ESP32, or Raspberry Pi**, please refer to the official Waveshare documentation:

https://www.waveshare.com/wiki/LCD1602_I2C_Module_(Blue)#C

This LCD module supports **two lines of text** and multiple **font configurations**.
This library uses the **5x8 dot font configuration by default**.

To change the font configuration, edit the `function_set` macro bitmask inside the `.c` or `.cpp` source file.

Relevant documentation for the LCD controller and bitmask configuration can be found below:

* https://files.waveshare.com/wiki/LCD1602-I2C-Module/LCD1602_I2C_Module.pdf
* https://files.waveshare.com/wiki/LCD1602-I2C-Module/AIP31068L.pdf

---

# 1. About

This directory contains the driver used to interface with the **Waveshare LCD1602 I2C Module V2.0** for the buoy project.

The LCD module allows verification of each subsystem during **system startup**. This enables immediate identification of faults, which is especially important when deploying the prototype in remote environments.

Additionally, the LCD is used for **debugging**, which is essential during subsystem integration—particularly for components that cannot easily be stepped through using the **STM32CubeIDE debugger**, such as the camera capture subsystem.

---

# 2. Part Information

The LCD module used is the **Waveshare Blue 16x2 I2C LCD Module (3.3V/5V) with Backlight Controller**.

The display is available with multiple backlight colours including **blue, green, and grey**.

### Product Links

* Waveshare Blue 16x2 I2C LCD Module
  https://thepihut.com/products/waveshare-blue-16x2-i2c-lcd-module-3-3v-5v-with-backlight-control

* Waveshare Green 16x2 I2C LCD Module
  https://thepihut.com/products/waveshare-green-16x2-i2c-lcd-module-3-3v-5v-with-backlight-control

* Waveshare Grey 16x2 I2C LCD Module
  https://thepihut.com/products/waveshare-grey-16x2-i2c-lcd-module-3-3v-5v-with-backlight-control

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/LCD_Module/LCD_Module_image.webp" width="400">
</p>

<p align="center"><b>Figure 1:</b> Waveshare LCD Module</p>

---

# 3. Tested Hardware

This driver has been tested with:

* **STM32F401RCT6**
* **Waveshare LCD1602 I2C Module V2.0**

---

# 4. Development Environment

| Tool             | Version             |
| ---------------- | ------------------- |
| IDE              | STM32CubeIDE        |
| Version          | 2.0.0               |
| Build            | 26820_20251114_1348 |
| Vendor           | STMicroelectronics  |
| MCU              | STM32F401RCT6       |
| Firmware Library | STM32 HAL           |

---

# 5. Hardware Connections

The LCD module communicates with the STM32 using an **I2C interface**.

## STM32 ↔ LCD Connections

| LCD Pin | STM32 Peripheral | Description   |
| ------- | ---------------- | ------------- |
| VCC     | 3.3V / 5V        | Power supply  |
| GND     | GND              | Ground        |
| SCL     | I2C_SCL          | I2C clock     |
| SDA     | I2C_SDA          | I2C data line |

---

## Example STM32F401 Pin Mapping

| STM32 Pin | Peripheral | Function  |
| --------- | ---------- | --------- |
| PB6       | I2C1_SCL   | I2C clock |
| PB7       | I2C1_SDA   | I2C data  |

---

# 6. API

## Functions

> Note: The C++ implementation uses the same functions as the C API.
> The only difference is that `lcd_startup(..)` is handled automatically by the **class constructor**.

| Function                                             | Description                                                  |
| ---------------------------------------------------- | ------------------------------------------------------------ |
| `lcd_startup(I2C_HandleTypeDef *i2c_pin_assignment)` | Configures the LCD and backlight, and stores the I2C handle. |
| `set_brightness(uint8_t value)`                      | Sets backlight brightness (0–100%).                          |
| `set_led_mode(uint8_t mode)`                         | Sets LED mode: steady (`0x00`) or breathing (`0x20`).        |
| `display_on(void)`                                   | Turns the LCD display on.                                    |
| `display_off(void)`                                  | Turns the LCD display off.                                   |
| `clear(void)`                                        | Clears the display and resets the cursor.                    |
| `set_cursor(uint8_t col, uint8_t row)`               | Moves the cursor to a specific column and row.               |
| `data(uint8_t value)`                                | Sends a single character to the display.                     |
| `send_string(const char *str)`                       | Sends a string to the display.                               |
| `no_cursor(void)`                                    | Hides the cursor.                                            |
| `cursor(void)`                                       | Shows the cursor.                                            |
| `scroll_display_left(void)`                          | Scrolls display left.                                        |
| `scroll_display_right(void)`                         | Scrolls display right.                                       |
| `left_to_right(void)`                                | Sets text direction left-to-right.                           |
| `right_to_left(void)`                                | Sets text direction right-to-left.                           |
| `no_autoscroll(void)`                                | Disables automatic scrolling.                                |
| `autoscroll(void)`                                   | Enables automatic scrolling.                                 |
| `create_char(uint8_t location, uint8_t charmap[])`   | Creates a custom character (CGRAM location 0–7).             |

---

# 7. Enums

## `LCD_STATUS`

This enum is used for **error handling and debugging**.

| Value                              | Description                                                  |
| ---------------------------------- | ------------------------------------------------------------ |
| `LCD_OK`                           | Operation successful                                         |
| `LCD_ERROR_I2C`                    | I2C communication failure when sending data or commands      |
| `LCD_ERROR_I2C_REG`                | I2C communication failure during backlight configuration     |
| `LCD_ERROR_FUNCTION_SET`           | Failed to configure LCD function settings (font, mode, etc.) |
| `LCD_ERROR_DISPLAY_ON`             | Failed to enable the LCD display                             |
| `LCD_ERROR_CLEAR_SCREEN`           | Failed to clear the display                                  |
| `LCD_ERROR_ENTRY_MODE`             | Failed to configure cursor entry mode                        |
| `LCD_ERROR_SETTING_BRIGHTNESS`     | Invalid brightness value passed to `set_brightness(..)`      |
| `LCD_ERROR_SETTING_BACKLIGHT_MODE` | Invalid mode passed to backlight mode function               |
| `LCD_ERROR_SENDING_STRING`         | Failed to send a string to the display                       |
| `LCD_ERROR_CREATE_CHAR`            | Failed to upload custom character data                       |
| `LCD_ERROR_BACKLIGHT_INIT`         | Backlight initialization failed (constructor check)          |
| `LCD_ERROR_LCD_INIT`               | LCD initialization failed (constructor check)                |

---

# 13. Code Flow

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/LCD_Module/LCD%20Flow%20Diagram.jpeg" width="800">
</p>

<p align="center"><b>Figure 2:</b> LCD Processing Pipeline</p>

### Processing Pipeline

This is the typical process used when writing text to the display.

1. Configure the LCD backlight controller via **I2C**
2. Configure the LCD controller via **I2C**
3. Set the cursor position via **I2C**
4. Send the string to the LCD via **I2C**
5. Clear the display when needed

For advanced features such as **scrolling animations or custom characters**, see the examples.

---

# 14. Dependencies

* **STM32 HAL Library**
* **STM32 I2C Peripheral**

---

# 15. Example Usage (C)

```c
lcd_startup(&hi2c1);

// Set cursor to first row
set_cursor(0, 0);
send_string("Waveshare");

// Set cursor to second row
set_cursor(0, 1);
send_string("Hello, World!");
```

---

# 16. Example Usage (C++)

```cpp
WaveShareLCD lcd(&hi2c1);

lcd.set_brightness(50);

// First row
lcd.set_cursor(0,0);
lcd.send_string("Waveshare");

// Second row
lcd.set_cursor(0,1);
lcd.send_string("Hello, World!");
```
