/*
 * sensor_registry.cpp
 *
 *   1. Create its driver files.
 *   2. Add init/read adapter functions and a Sensor const below.
 *   3. Add a pointer to REGISTRY[] in ascending bit order.
 */
#include "main.h"    // for hi2c1
#include <cstring>   // for memcpy
#include <stdint.h>  // for uint8_t, uint16_t, uint32_t
#include <stddef.h>
#include "sensor_registry.h"
#include "sensor.h"
#include "ltr390_driver.hpp"
#include "tilt_sensor.hpp"
#include "gps.h"
#include "emi.h"
#include "SHT4x.h"
#include "BatteryMonitor.h"


static Ltr390Driver ltr_driver{&hi2c3};

static void uv_init(void) {
    (void)ltr_driver.init();
}

static void uv_read(uint8_t *out) {
	(void)ltr_driver.calculate_uv_index();
	(void)ltr_driver.calculate_lux();
	float uvi = ltr_driver.get_uv_index();
	float lux = ltr_driver.get_lux();
	memcpy(out,     &uvi, 4);
	memcpy(out + 4, &lux, 4);
}

static const Sensor uv_sensor = {
    .bit  = 0,
    .size = 8,
    .name = "UV",
    .init = uv_init,
    .read = uv_read,
};


static TiltSensor tilt_driver(&hspi3, GPIOB, GPIO_PIN_8);

static void tilt_init(void) {
    // calibrate_flat_offset() takes 50 samples on a flat surface.
    // Only call this in a lab — see README warning.
    if (tilt_driver.init() == TILT_OK) {
        //tilt_driver.calibrate_flat_offset();
    }
}

static void tilt_read(uint8_t *out) {
    tilt_driver.update();
    float x = tilt_driver.get_x_deg();
    float y = tilt_driver.get_y_deg();
    memcpy(out,     &x, 4);
    memcpy(out + 4, &y, 4);
}

static const Sensor tilt_sensor = {
    .bit  = 4,
    .size = 8,
    .name = "Tilt",
    .init = tilt_init,
    .read = tilt_read,
};


static void gps_init_cb(void) { gps_init(); }

static void gps_read_cb(uint8_t *out) { gps_read(out); }

static const Sensor gps_sensor = {
    .bit  = 3,
    .size = 8,
    .name = "GPS",
    .init = gps_init_cb,
    .read = gps_read_cb,
};


static BatteryMonitor battery_monitor(&hi2c3);
static bool battery_init_ok = false;

static void battery_init_cb(void) {
    battery_init_ok = (battery_monitor.init(25.0f) == BATTERY_OK);
}

static void battery_read(uint8_t *out) {
    float pct = 0.0f, v = 0.0f, ma = 0.0f;
    if (battery_init_ok && battery_monitor.update(25.0f) == BATTERY_OK) {
        pct = battery_monitor.get_percentage();
        v   = battery_monitor.get_voltage();
        ma  = battery_monitor.get_current();
    }
    memcpy(out,     &pct, 4);   // bytes  0–3:  SOC percentage (float)
    memcpy(out + 4, &v,   4);   // bytes  4–7:  bus voltage in V (float)
    memcpy(out + 8, &ma,  4);   // bytes 8–11:  current in mA  (float)
}

static const Sensor battery_sensor = {
    .bit  = 5,
    .size = 12,          // 3 × float32
    .name = "Battery",
    .init = battery_init_cb,
    .read = battery_read,
};


static void emi_init_cb(void) { emi_init(); }

static void emi_read_cb(uint8_t *out) { emi_read(out); }

static const Sensor emi_sensor = {
	.bit = 6,
	.size = 20,        // I, Q, Magnitude, Phase, IQ Phase
	.name = "EMI",
	.init = emi_init_cb,
	.read = emi_read_cb,
};


static void sht45_init(void) {
    SHT4x_Init(&hi2c3);
}

static void sht45_read(uint8_t *out) {
    SHT4x_Data_t d;
    if (!SHT4x_GetEvent(&d)) {
    	memset(out, 0, 8); return;
    }
    memcpy(out,     &d.temperature, 4);   // float32, °C
    memcpy(out + 4, &d.humidity,    4);   // float32, %RH
}

static const Sensor sht45_sensor = {
    .bit  = 7,
    .size = 8,
    .name = "SHT45",
    .init = sht45_init,
    .read = sht45_read,
};

static const Sensor *const REGISTRY[] = {
		&uv_sensor,		// bit 0
		&gps_sensor,	// bit 3
		&tilt_sensor,	// bit 4
		&battery_sensor,// bit 5
		&emi_sensor, 	// bit 6
		&sht45_sensor,  // bit 7

};
// ... rest of registry (PUT THEM IN ORDER OF BITS)

static const uint8_t REGISTRY_LEN = sizeof(REGISTRY) / sizeof(REGISTRY[0]);


void sensors_init(void)
{
    for (uint8_t i = 0; i < REGISTRY_LEN; i++) {
        if (REGISTRY[i]->init != NULL) {
            REGISTRY[i]->init();
        }
    }
}


/* Walks through registry and checks if bit is set in bitmask
*  If set, call sensor read function and fill paylod with data
*/
uint16_t build_sensor_payload(uint32_t mask, uint8_t *buf)
{
    uint16_t offset = 0;
    for (uint8_t i = 0; i < REGISTRY_LEN; i++) {
        const Sensor *s = REGISTRY[i];
        if (mask & (1UL << s->bit)) {
            s->read(buf + offset);
            offset += s->size;
        }
    }
    return offset;
}
