/*
 * db_sensor_registry.cpp  —  DB (daughter board)
 *
 * Mirrors MB sensor_registry.cpp in structure
 *
 *   1. Create its driver files.
 *   2. Add init/read adapter functions and a Sensor const below.
 *   3. Add a pointer to REGISTRY[] in ascending bit order.
 *
 */
#include "main.h"
#include <cstring>
#include <stdint.h>
#include <stddef.h>
#include "TurbiditySensor.h"
#include "db_sensor_registry.h"
#include "db_sensor.h"
#include "SHT4X.h"
#include "temperature_sensor.h"
#include "imu_sensor.hpp"

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
    .bit  = 11,
    .size = 8,
    .name = "SHT45",
    .init = sht45_init,
    .read = sht45_read,
};

static void thermistor_init(void)
{
    temp_sensor_init();
}

static void thermistor_read(uint8_t *out)
{
    uint8_t len = 0;
    temp_sensor_read(out, &len);
}

static const Sensor temp_sensor_ext = {
    .bit  = 12,
    .size = 16,   /* 4 × float32 */
    .name = "TemperatureExt",
    .init = thermistor_init,
    .read = thermistor_read,
};

static TurbiditySensor *turb_ptr = nullptr;

static void turbidity_init(void)
{
	static TurbiditySensor inst(&hadc2, GPIOC, GPIO_PIN_12);
    turb_ptr = &inst;
}

static void turbidity_read(uint8_t *out)
{
    float ntu = turb_ptr->readTurbidity();
    memcpy(out, &ntu, sizeof(float));
}

static const Sensor turbidity_sensor = {
    .bit  = 13,
    .size = 4,       // one float32 (NTU)
    .name = "Turbidity",
    .init = turbidity_init,
    .read = turbidity_read,
};


static ImuDB *imu_ptr = nullptr;

static void imu_init(void)
{
    static ImuDB inst(&hi2c3, (0x68 << 1));
    imu_ptr = &inst;
    imu_ptr->initialize();
}

static void imu_read(uint8_t *out)
{
    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
    if (imu_ptr != nullptr) {
        imu_ptr->read_angles(roll, pitch, yaw);
    }
    memcpy(out + 0, &roll,  4);
    memcpy(out + 4, &pitch, 4);
    memcpy(out + 8, &yaw,   4);
}

static const Sensor imu_sensor = {
    .bit  = 14,
    .size = 12,   // 3 × float32 (roll °, pitch °, yaw °)
    .name = "IMU",
    .init = imu_init,
    .read = imu_read,
};

static const Sensor *const REGISTRY[] = {
	&sht45_sensor,     // bit 11
	&temp_sensor_ext,  // bit 12
	&turbidity_sensor, // bit 13
	&imu_sensor,       // bit 14
};

static const uint8_t REGISTRY_LEN = sizeof(REGISTRY) / sizeof(REGISTRY[0]);

/* =========================================================================
 * Public API  —  called by hub.cpp, identical signature to MB
 * =========================================================================*/
void sensors_init(void)
{
    for (uint8_t i = 0; i < REGISTRY_LEN; i++) {
        if (REGISTRY[i]->init != NULL) {
            REGISTRY[i]->init();
        }
    }
}

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
