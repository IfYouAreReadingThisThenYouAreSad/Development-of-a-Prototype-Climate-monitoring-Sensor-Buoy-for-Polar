# Sea Ice Thickness Sensor using EMI (STM32)

---

# 1. Overview

The USP of the buoy was that it can measure sea ice non-destructively, which no other buoy on the market does to date. An electromagnetic sensing system was custom made in order to do this, shown below in a testing photo. This works by driving a transmitter coil using a power amplifier with a high frequency changing voltage, which creates a magnetic field that then produces eddy currents in the sea water. These eddy currents create a magnetic field that opposes the magnetic field of the transmitter coil (Lenz's law). This secondary changing field is then picked up by the receiver coil, which allows us to infer sea ice thickness from the magnitude and in-phase response, as shown in Figure 2 below. (Note: a bucking coil of 1 turn was added to the system to stop any primary coupling between the transmitter (Tx) coil and receiver (Rx) coil, which is not shown on the explanation diagram below.)

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/Sea%20Ice%20Thickness%20(EMI)/EMI%20Process%20Diagram.png" width="400">
</p>

<p align="center"><b>Figure 1:</b> EMI interaction with seawater.</p>

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/Sea%20Ice%20Thickness%20(EMI)/EMI%20System%20Testing%20photo.png" width="400">
</p>

<p align="center"><b>Figure 2:</b> EMI system photo.</p>

A custom EMI board was made for this, where a DAC signal and +15V/-15V rails are fed into the PCB. The DAC on the STM32F405RGT6 produces a 20 kHz, 1.2 V peak-to-peak signal, which is passed into the EMI PCB and then amplified up using a power amplifier and sent to the Tx coil, where the current is measured by a 1Ω shunt, as shown in Figure 3. The Rx coil is fed into the PCB, where parallel resistors and capacitors were added to help increase the sensitivity of the system. The bucking coil was also fed into the Rx header, going to the -Vin of the instrumentation amplifier to subtract the primary field coupling, as shown in Figure 4. Additionally, a 1.65V reference was created using the STM32F405RGT6's 3.3V ADC reference, through a potential divider and unity gain op-amp, to create a low impedance 1.65V reference and utilise the full range of the system's ADC, as shown in Figure 5.

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/Sea%20Ice%20Thickness%20(EMI)/Tx%20Drive%20Circuit.png" width="400">
</p>

<p align="center"><b>Figure 3:</b> Power Amplifier (Tx Drive) and Shunt Circuit.</p>

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/Sea%20Ice%20Thickness%20(EMI)/RX%20and%20Bucking%20Circuit.png" width="400">
</p>

<p align="center"><b>Figure 4:</b> Rx Resonating and Bucking Subtraction Circuit.</p>

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/Sea%20Ice%20Thickness%20(EMI)/1.65V%20circuit.png" width="400">
</p>

<p align="center"><b>Figure 5:</b> 1.65V Reference Circuit.</p>

---

# 2. Hardware

The system uses the:

**OPA541AP Texas Instruments, Operational Amplifier, 2 MHz, 11-Pin 40V TO-220 (Power Amplifier)**

### Product Link
https://uk.rs-online.com/web/p/op-amps/1977445?gb=a

### Datasheet
https://www.ti.com/lit/ds/symlink/opa541.pdf?ts=1781225701324

**INA111AP Texas Instruments Instrumentation Amplifier, ±1000 μV Offset 2 MHz, 8-Pin 4.5 mA PDIP (Instrumentation Amplifier)**

### Product Link
https://uk.rs-online.com/web/p/instrumentation-amplifiers/1977135?gb=a

### Datasheet
https://www.ti.com/lit/ds/symlink/ina111.pdf?ts=1781246890532&ref_url=https%253A%252F%252Fwww.google.com%252F

**OPA277PA Texas Instruments, Operational Amplifier, 1 MHz, 8-Pin 18V PDIP (Op-Amp)**

### Product Link
https://uk.rs-online.com/web/p/op-amps/3071667

### Datasheet
https://www.ti.com/lit/ds/sbos079c/sbos079c.pdf?ts=1781237728977

---

# 3. CubeMX Settings

Note: to produce a 20 kHz signal with the DAC, the clock settings and prescaler will have to be changed, with the addition of using a timer in order to trigger the DAC. For the ADCs to record the shunt and Rx voltage, a rank was used with DMA for both ADCs and the DAC. DMA must be used, especially for the DAC, to generate the 20 kHz, 1-1.2 V peak-to-peak sine wave, or it will be distorted or at the wrong frequency.

---

# 4. Hardware Connections

The EMI sensor is read via the **STM32 DAC and ADC** peripherals, clocked in lock-step by a timer.

## STM32 ↔ EMI PCB Connections

| STM32 Pin | Peripheral | Description                              |
|-----------|------------|-------------------------------------------|
| PA4       | DAC_OUT1   | 20 kHz sine drive signal to Tx power amp   |
| PC3       | ADC2_IN13  | Rx coil / shunt output, lock-in capture    |
| TIM6      | Timer      | Clocks both DAC DMA and ADC2 DMA in lock-step |

**Bit-conflict note:** EMI is registered at bit 6 in the sensor registry. The UV/lux sensor was also at bit 6 — it must be moved to bit 5 (or any other free bit) in `sensor_registry.cpp`.

---

# 5. API

## Initialisation

| Function | Description |
|----------|-------------|
| `void emi_init(void)` | Builds the DAC sine LUT and the reference I/Q LUTs. Called once from `sensors_init()`. |

---

## Public Functions

| Function | Description |
|----------|-------------|
| `void emi_init(void)` | Builds the DAC sine LUT and reference I/Q LUTs used for the lock-in calculation. |
| `void emi_read(uint8_t *out)` | Runs `EMI_NUM_REPS` timer-triggered DMA captures, averages the corrected lock-in results, and writes 20 bytes to `out`. Blocking — returns after all repetitions complete. At default parameters (50 reps, 150 cycles, 128-point LUT, ~2.1 MHz sample rate) this takes approximately 460 ms. |

---

# 6. Output Payload

`emi_read()` writes 20 bytes — five IEEE-754 `float32` values, in order:

| Bytes   | Value | Description |
|---------|-------|-------------|
| 0–3     | `avg_corr_i` | Average corrected in-phase (I) component |
| 4–7     | `avg_corr_q` | Average corrected quadrature (Q) component |
| 8–11    | `avg_corr_mag` | Average corrected magnitude |
| 12–15   | `avg_corr_phase_deg` | Average corrected phase, in degrees (circular mean) |
| 16–19   | `avg_corr_phase_from_iq` | Average phase derived from I/Q, in degrees |

These match the printf order in `print_average_results()` from the EMITesting reference project.

---

# 7. Code Flow

## Processing Steps

1. **Initialisation (run once)**
   - Builds the DAC sine LUT used to drive the Tx coil
   - Builds the reference I/Q LUTs used for the lock-in calculation

2. **Measurement (per `emi_read()` call)**
   - TIM6 triggers DMA-driven output of the sine LUT to the DAC (PA4 → Tx power amp)
   - TIM6 simultaneously triggers DMA-driven ADC2 capture of the Rx/shunt signal (PC3)
   - `EMI_NUM_REPS` capture cycles are performed (default 50 reps × 150 cycles, 128-point LUT, ~2.1 MHz sample rate)

3. **Lock-in Processing & Output**
   - Each capture is correlated against the reference I/Q LUTs to extract the in-phase (I) and quadrature (Q) components
   - I, Q, magnitude, and phase are corrected and averaged across all repetitions (phase using a circular mean)
   - Writes the five resulting `float32` values (I, Q, magnitude, phase, phase-from-I/Q) into the 20-byte output buffer

---

# 8. Dependencies

- STM32 HAL Library
- STM32 DAC Peripheral (DAC_CHANNEL_1)
- STM32 ADC2 Peripheral
- TIM6
- DMA (required for both DAC and ADC2)

See `IOC_SETUP.md` (or the comment block in `emi.c`) for the full `.ioc` configuration prerequisites.

---

# 9. Example Usage (C)

```c
uint8_t emi_buf[20];

emi_init();   // called once from sensors_init()

while (1)
{
    emi_read(emi_buf);   // blocking, ~460 ms at default parameters

    float avg_corr_i, avg_corr_q, avg_corr_mag;
    float avg_corr_phase_deg, avg_corr_phase_from_iq;

    memcpy(&avg_corr_i,             &emi_buf[0],  4);
    memcpy(&avg_corr_q,             &emi_buf[4],  4);
    memcpy(&avg_corr_mag,           &emi_buf[8],  4);
    memcpy(&avg_corr_phase_deg,     &emi_buf[12], 4);
    memcpy(&avg_corr_phase_from_iq, &emi_buf[16], 4);

    BSP_LED_Toggle(LED_GREEN);
    HAL_Delay(500);
}
```
