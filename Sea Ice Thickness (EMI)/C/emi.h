/*
 * emi.h
 *
 * Lock-in EMI sensor — DMA + TIM6 triggered capture for STM32F405.
 *
 * Hardware:
 *   PA4  DAC_OUT1  →  sine drive signal (hdac, DAC_CHANNEL_1)
 *   PC3  ADC2_IN13 →  coil output / received signal (hadc2)
 *   TIM6           →  clocks both DAC DMA and ADC2 DMA in lock-step
 *
 * Payload written by emi_read() — 20 bytes, five IEEE-754 floats in order:
 *
 *   [0..3]   avg corrected I          → avg_corr_i
 *   [4..7]   avg corrected Q          → avg_corr_q
 *   [8..11]  avg corrected magnitude  → avg_corr_mag
 *   [12..15] avg corrected phase, deg → avg_corr_phase_deg   (circular mean)
 *   [16..19] avg phase from I/Q, deg  → avg_corr_phase_from_iq
 *
 * These match the printf order in print_average_results() from the
 * EMITesting reference project.
 *
 * .ioc prerequisites — see IOC_SETUP.md or the comment block in emi.c.
 *
 * Bit-conflict note:
 *   EMI is registered at bit 6.  The UV/lux sensor was also at bit 6;
 *   move it to bit 5 (or any free bit) in sensor_registry.cpp.
 */

#ifndef EMI_H
#define EMI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Build the DAC sine LUT and reference I/Q LUTs.
 *        Called once from sensors_init().
 */
void emi_init(void);

/**
 * @brief Run EMI_NUM_REPS timer-triggered DMA captures, average the
 *        corrected lock-in results, and write 20 bytes to @p out.
 *
 *        Blocking — returns after all repetitions complete.
 *        At the default parameters (50 reps, 150 cycles, 128-pt LUT,
 *        ~2.1 MHz sample rate) this takes approximately 460 ms.
 *
 * @param out  Buffer of at least 20 bytes.
 */
void emi_read(uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* EMI_H */
