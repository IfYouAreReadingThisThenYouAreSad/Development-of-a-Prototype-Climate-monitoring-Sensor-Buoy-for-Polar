# Battery Monitor Module (STM32)


If you are looking for libraries for **Arduino, or Raspberry Pi Pico**, please refer to the official Waveshare documentation:

https://learn.adafruit.com/adafruit-ina219-current-sensor-breakout/arduino-code

---

# 1. About

This directory contains the driver used to interface with the **INA219 High Side DC Current Sensor Breakout** for the buoy project.

The INA219 Battery Monitor module allows monitorying of battery charge over I2C by monitory current.
Such module is critical for energy consicence project where energy is limited and needs to be monitored affectively.


---

# 2. Part Information

The Battery Monitor  module used is the **INA219 High Side DC Current Sensor Breakout - 26V ±3.2A Max - STEMMA QT**.

### Product Links

* INA219 High Side DC Current Sensor Breakout - 26V ±3.2A Max - STEMMA QT
  https://www.adafruit.com/product/904?srsltid=AfmBOoq6CFtvwWGXu99H22Pesroya0Lne_yIdSLup4lU1yT_X6TmCxsU


<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/BatteryMonitor_Module/ina219-high-side-dc-current-sensor-breakout-26v-3-2a-max-stemma-qt-adafruit-ada904-28609974796483_1000x.jpg" width="400">
</p>

<p align="center"><b>Figure 1:</b> Battery Monitor Module</p>

---


# 5. Hardware Connections

The INA219 module communicates with the STM32 using an **I2C interface**.

## STM32 ↔ INA219 Connections

| LCD Pin |  Peripheral | Description   |
| ------- | ---------------- | ------------- |
| VCC     | 3.3V             | Power to Module  |
| VIn+     |   +Termainal_Battery           | Battery  |
| VIn-     |   System_load            | VIn+ to VIN- then to the Load |
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



| Function                                             | Description                                                  |
| ---------------------------------------------------- | ------------------------------------------------------------ |
| `BatteryStatus init(float current_temp_c)`           |  Initialises the monitor by reading the current voltage and estimating starting state-of-charge. |
| `BatteryStatus update(float current_temp_c)`         | Updates state-of-charge by integrating current draw and applying a voltage anchor correction.            |
| `float get_percentage()`                             | Returns the last calculated state-of-charge percentage.        |
| `float get_voltage()`                                | Returns the last measured battery bus voltage.                                   |
| `float get_current()`                                | Returns the last measured current draw.                                  |


---

# 7. Enums

## `BatteryStatus`

This enum is used for **error handling and debugging**.

| Value                              | Description                                                  |
| ---------------------------------- | ------------------------------------------------------------ |
| `BATTERY_OK`                       | Operation successful                                         |
| `BATTERY_I2C_ERROR`                | I2C communication failure when sending data or commands      |
| `BATTERY_DATA_FAULT`               | I2C communication failure during backlight configuration     |


---

# 13. Code Flow

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/LCD_Module/LCD%20Flow%20Diagram.jpeg" width="800">
</p>

<p align="center"><b>Figure 2:</b> INA219 Processing Pipeline</p>

### Processing Pipeline

This is the typical process used when for geting battery information using the upadate function.
0. Inization of the battery (this is run once) caliabreate the battery with an extranl temp reading and estimates its charge.
1. Recalibrate for temprature
2. Read current and Voltage
3. Columb counting and Drift correction
4. Calculate how much mah are left and update the battery percentage



---

# 14. Dependencies

* **STM32 HAL Library**
* **STM32 I2C Peripheral**

---


# 16. Example Usage (C++)

```cpp
// 1. Instantiate the battery monitor and link it to the hardware I2C peripheral
    BatteryMonitor system_battery(&hi2c3);


    float current_buoy_temp = 25.0f;

    // 2. Initialize the sensor (handles derating, baseline SoC, and I2C connection check)
    BatteryStatus init_status = system_battery.init(current_buoy_temp);

    if (init_status != BATTERY_OK) {
        // Optional: Handle boot-up sensor failure (e.g., turn on a red LED, log error)
    }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  float current_buoy_temp = 25.0f;

	  // Pass the live temperature into the update function
	  if (system_battery.update(current_buoy_temp) == BATTERY_OK) {
		float final_percentage = system_battery.get_percentage();
		float current_draw_ma  = system_battery.get_current();
		float current_volts    = system_battery.get_voltage();

		(void)final_percentage;
		(void)current_draw_ma;
		(void)current_volts;
	  } else {
		// Handle BATTERY_I2C_ERROR or BATTERY_DATA_FAULT
	  }

	  HAL_Delay(500);
  }
```
