# Battery Monitor Module (STM32)

If you are looking for libraries for **Arduino or Raspberry Pi Pico**, please refer to the official Adafruit documentation:

https://learn.adafruit.com/adafruit-ina219-current-sensor-breakout/arduino-code

---

# 1. Overview

This directory contains a driver for interfacing with the **INA219 High-Side DC Current Sensor** over I2C using an STM32 microcontroller.

The INA219 enables real-time measurement of:
- Bus voltage  
- Current draw  
- Power consumption  

This module is used in the buoy project to monitor battery usage and estimate the **state of charge (SoC)**. Accurate energy monitoring is critical in low-power systems where energy availability is limited.

---

# 2. Hardware

The system uses the:

**INA219 High Side DC Current Sensor Breakout – 26V ±3.2A (STEMMA QT)**

### Product Link
https://www.adafruit.com/product/904

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/BatteryMonitor_Module/ina219-high-side-dc-current-sensor-breakout-26v-3-2a-max-stemma-qt-adafruit-ada904-28609974796483_1000x.jpg" width="400">
</p>

<p align="center"><b>Figure 1:</b> INA219 Current Sensor Module</p>

---

# 3. Hardware Connections

The INA219 communicates via the **I2C interface**.

## STM32 ↔ INA219 Connections

| INA219 Pin | STM32 / System | Description                  |
|------------|----------------|------------------------------|
| VCC        | 3.3V           | Module power supply          |
| GND        | GND            | Common ground                |
| SCL        | I2C_SCL        | I2C clock line              |
| SDA        | I2C_SDA        | I2C data line               |
| VIN+       | Battery +      | Connected to battery supply |
| VIN-       | Load +         | Output to system load       |

> ⚠️ Current is measured across the internal shunt resistor between VIN+ and VIN-.

---

## Example STM32F401 Pin Mapping

| STM32 Pin | Peripheral | Function  |
|----------|-----------|----------|
| PB6      | I2C1_SCL  | I2C clock |
| PB7      | I2C1_SDA  | I2C data  |

---

# 4. API

## Constructor

| Function | Description |
|----------|------------|
| `BatteryMonitor(I2C_HandleTypeDef* handle)` | Initialises the driver with the STM32 I2C peripheral. Internal variables are reset during construction. |

---

## Public Functions

| Function | Description |
|----------|------------|
| `BatteryStatus init(float current_temp_c)` | Initialises the sensor, performs calibration, and estimates initial state-of-charge using voltage. |
| `BatteryStatus update(float current_temp_c)` | Updates measurements and state-of-charge using current integration (coulomb counting) with voltage correction. |
| `float get_percentage()` | Returns the latest state-of-charge (%). |
| `float get_voltage()` | Returns the measured bus voltage (V). |
| `float get_current()` | Returns the measured current (mA or A depending on implementation). |

---

# 5. Enums

## `BatteryStatus`

Used for error handling and debugging.

| Value | Description |
|------|------------|
| `BATTERY_OK` | Operation successful |
| `BATTERY_I2C_ERROR` | I2C communication failure |
| `BATTERY_DATA_FAULT` | Invalid or corrupted sensor data |

---

# 6. Code Flow

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/BatteryMonitor_Module/INA219%20Diagram.jpeg" width="800">
</p>

<p align="center"><b>Figure 2:</b> INA219 Processing Pipeline</p>

## Processing Steps

1. **Initialisation (run once)**
   - Sensor calibration  
   - Initial voltage measurement  
   - Estimate initial state-of-charge  

2. **Temperature Compensation**
   - Adjust parameters based on measured temperature  

3. **Measurement**
   - Read bus voltage  
   - Read current  

4. **Coulomb Counting**
   - Integrate current over time to estimate energy usage  

5. **Drift Correction**
   - Apply voltage-based correction to reduce long-term drift  

6. **State Update**
   - Update remaining capacity and percentage  

---

# 7. Dependencies

- STM32 HAL Library  
- STM32 I2C Peripheral  

---

# 8. Example Usage (C++)

```cpp
// Instantiate the battery monitor using I2C handle
BatteryMonitor system_battery(&hi2c3);

float current_buoy_temp = 25.0f;

// Initialise the sensor
BatteryStatus init_status = system_battery.init(current_buoy_temp);

if (init_status != BATTERY_OK) {
    // Handle initialisation error
}

// Main loop
while (1)
{
    float current_buoy_temp = 25.0f;

    if (system_battery.update(current_buoy_temp) == BATTERY_OK) {
        float battery_percentage = system_battery.get_percentage();
        float current_draw       = system_battery.get_current();
        float battery_voltage    = system_battery.get_voltage();

        (void)battery_percentage;
        (void)current_draw;
        (void)battery_voltage;
    } else {
        // Handle errors (I2C or data fault)
    }

    HAL_Delay(500);
}
