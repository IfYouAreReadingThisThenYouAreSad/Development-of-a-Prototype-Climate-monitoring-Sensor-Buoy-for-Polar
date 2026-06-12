# Salinity (STM32)

---

# 1. Overview

A custom made salinity sensor was made for the buoy to save money and to help expand the functionality of traditional salinity sensors. A 4-probe salinity sensor was used, which leads to better long-term measurements than a traditional 2-probe sensor. The sensor calculates salinity through the cell constant, which was found through calibration (read the final report for more information). Additionally, the salinity sensor has also been calibrated for some temperature change, but more work needs to be done on this.

---

# 2. Hardware

The sensor uses the daughter board's DAC. This is run through a low-pass filter, or can be jumped with a 0Ω resistor so it acts as a unity gain amplifier. Next, the signal goes through an AC coupling stage to remove any DC components from the DAC. It is then passed through a shunt resistor before going to the outer electrodes. The voltage across the shunt goes through an instrumentation amplifier, which adds gain and shifts it up to 1.65V to use the ADC's full range. The inner electrodes go through a duplicate instrumentation amplifier arrangement. All of this is shown in the image below.

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/Salinity/Salinity%20schematic.png" width="800">
</p>

<p align="center"><b>Figure 1:</b> Salinity sensor signal chain — DAC excitation, AC coupling, shunt, and instrumentation amplifier stages.</p>

---

# 3. Hardware Connections

The salinity sensor is driven and read via the **STM32 DAC and ADC** peripherals, not I2C.

## STM32 ↔ Salinity Circuit Connections

| Signal | STM32 / System | Description                                      |
|--------|-----------------|---------------------------------------------------|
| DAC_OUT2 | DAC Channel 2 | 7 Hz sine excitation signal, sent to outer electrodes via shunt |
| ADC1_IN9 | ADC1 (master) | Current channel (shunt voltage), DMA captured     |
| ADC2_IN14 | ADC2 (slave)  | Voltage channel (inner electrode voltage), DMA captured |
| TIM6    | Timer          | Triggers the DAC sine output (448 Hz update rate, 64-point LUT) |
| TIM3    | Timer          | Triggers simultaneous dual ADC1/ADC2 sampling     |

---

# 4. API

This module is implemented directly in `main.c` rather than as a separate driver library, as only one measurement function is required.

## Public Functions

| Function | Description |
|----------|-------------|
| `ConductivityResult Conductivity_Measure(void)` | Runs a full conductivity/salinity measurement cycle: safety check, DAC excitation, dual-ADC DMA capture, and conversion to conductivity and salinity. Always check `.status` before using the returned values. |

---

## Important `#define` Constants

| Variable | Description |
|----------|-------------|
| `SINE_SAMPLES` | Number of points in the DAC sine LUT (64). |
| `SINE_FREQ_HZ` | Excitation frequency (7 Hz). |
| `DAC_MID_COUNT` / `DAC_AMP_COUNT` | DAC LUT mid-rail and amplitude counts, set for a 1Vpp sine centred at 1.65V on a 3.3V/12-bit DAC. |
| `CYCLES_TO_CAPTURE` | Number of excitation cycles captured per measurement (16). |
| `ADC_BUF_SIZE` | Total dual-word ADC samples captured per measurement (`CYCLES_TO_CAPTURE * SINE_SAMPLES` = 1024). |
| `VREF` / `ADC_FULL_SCALE` | ADC reference voltage (3.3V) and full-scale count (4095) used to convert raw counts to volts. |
| `ADC_SAFETY_THRESHOLD` | Raw ADC count (~3.22V) above which a probe is considered shorted before excitation starts. |
| `CONDUCTIVITY_SCALE` | Cell constant used to convert the current/voltage RMS ratio to conductivity (mS), found through calibration. |
| `SALINITY_SCALE` | Constant used to convert conductivity (mS) to salinity (g/L). |

---

## Enums and Structs

### `ConductivityStatus`

Returned by `Conductivity_Measure()` for error handling.

| Value | Description |
|-------|-------------|
| `COND_OK` | Measurement completed successfully. |
| `COND_FAULT_SHORT` | ADC read close to 3.3V before excitation started, or the voltage channel read near zero — indicates a short or open circuit on the probes. |
| `COND_FAULT_TIMEOUT` | DMA capture did not complete within the 3 second timeout. |

### `ConductivityResult`

Struct returned by `Conductivity_Measure()`.

| Member | Description |
|--------|-------------|
| `ConductivityStatus status` | Result status — must be checked before using the other fields. |
| `float adc1_rms_V` | AC-RMS voltage measured on ADC1 (current/shunt channel). |
| `float adc2_rms_V` | AC-RMS voltage measured on ADC2 (voltage channel). |
| `float conductivity_mS` | Calculated conductivity in mS. |
| `float salinity_gL` | Calculated salinity in g/L. |

---

# 5. Code Flow

## Processing Steps

1. **Safety check**
   - Performs one poll-mode ADC conversion on each channel before any excitation is applied
   - If either ADC reads above `ADC_SAFETY_THRESHOLD` (~3.22V), the probes are assumed shorted and the measurement aborts with `COND_FAULT_SHORT`
   - Re-initialises ADC1 and ADC2 back into their DMA-triggered configuration afterwards

2. **Excitation**
   - Builds a 64-point sine LUT for a 1Vpp, 1.65V-centred signal
   - Starts the DAC on Channel 2 via DMA, driven by TIM6 to produce a 7 Hz sine wave
   - Waits ~50 ms (half a cycle) for the signal to settle

3. **Capture**
   - Starts simultaneous dual-ADC DMA capture (ADC1 master / ADC2 slave), triggered by TIM3
   - Captures 1024 dual-word samples (16 full excitation cycles)
   - Waits for DMA completion, with a 3 second timeout (`COND_FAULT_TIMEOUT` if exceeded)

4. **Shutdown**
   - Stops TIM3, ADC DMA, TIM6, and the DAC in sequence
   - Parks the DAC output at mid-rail (1.65V) briefly before stopping it, to avoid a floating-output glitch

5. **Conversion**
   - Computes the AC-RMS voltage of each channel (DC bias removed via a two-pass mean/variance calculation)
   - If the voltage channel RMS is near zero, returns `COND_FAULT_SHORT` (likely open circuit)
   - Calculates conductivity: `conductivity_mS = (I_rms / V_rms) * CONDUCTIVITY_SCALE`
   - Calculates salinity: `salinity_gL = conductivity_mS * SALINITY_SCALE`

---

# 6. Dependencies

- STM32 HAL Library
- STM32 ADC1 and ADC2 Peripherals (dual-simultaneous DMA mode)
- STM32 DAC Peripheral (Channel 2, DMA mode)
- TIM6 (DAC trigger) and TIM3 (ADC trigger)
- `math.h` (for sine LUT generation)

---

# 7. Example Usage (C)

```c
ConductivityResult result = Conductivity_Measure();

if (result.status == COND_OK)
{
    float conductivity = result.conductivity_mS;
    float salinity     = result.salinity_gL;

    BSP_LED_Toggle(LED_GREEN);
}
else if (result.status == COND_FAULT_SHORT)
{
    // Probe short or open circuit detected
    Error_Handler();
}
else
{
    // ADC DMA timeout — check TIM3 and DMA configuration
    Error_Handler();
}
```
