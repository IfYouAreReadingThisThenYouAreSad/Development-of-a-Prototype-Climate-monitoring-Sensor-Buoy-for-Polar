# Hub and Main Execution Loop

# 1. About

This directory documents the **command dispatch layer** of the ArcticSense firmware, which links the transport layer (BLE / Iridium) to the sensor and camera subsystems.

The hub is the central coordination point on each board:

- The **Motherboard (MB) hub** (`hub.h` / `hub.cpp`) receives 5-byte commands from the ground-station GUI over USART1, decodes the sensor bitmask, collects data from MB-resident sensors via the sensor registry, forwards the bitmask to the **Daughterboard (DB)** over USART6, and transmits the assembled response back to the GUI in 20-byte fragments.
- The **Daughterboard (DB) hub** (`db_hub.h` / `db_hub.cpp`) mirrors the MB hub in structure. It receives forwarded commands from the MB over USART2, collects data from DB-resident sensors, and returns the payload directly to the MB.

Both hubs follow the same interrupt-safe pattern: a UART RX interrupt populates a volatile flag and bitmask, and the main loop polls and dispatches on each iteration.

---

# 2. Architecture

```
GUI (Python / bleak)
      │  5-byte command
      ▼
 [MB USART1] ── HM-10 BLE ──► MB hub
                                │
                    ┌───────────┴────────────┐
                    │                        │
             MB sensor registry       [MB USART6]
           (UV, GPS, Tilt, Battery,         │
            EMI, Temp, Humidity)      [DB USART2]
                    │                        │
                    │                  DB sensor registry
                    │           (Thermistors, Salinity, Turbidity
                    │                IMU, Temp, Humidity)
                    │                        │
                    └───────────┬────────────┘
                                │  assembled payload
                         MB USART1
                                │
                           GUI (response)
```

Camera captures (CAM1, CAM2) are handled exclusively by the MB hub. The DB hub has no camera path.

---

# 3. Tested Hardware

| Board | MCU |
|-------|-----|
| Motherboard (MB) | STM32F405RGT6 |
| Daughterboard (DB) | STM32F405RGT6 |

---

# 4. Development Environment

| Tool | Version |
|------|---------|
| IDE | STM32CubeIDE |
| Version | 2.0.0 |
| Build | 26820_20251114_1348 |
| Vendor | STMicroelectronics |
| MCU | STM32F405RGT6 |
| Firmware Library | STM32 HAL |

---

# 5. UART Assignments

## Motherboard

| UART | Pins | Baud | Role |
|------|------|------|------|
| USART1 | PA9 (TX), PA10 (RX) | 9600 | BLE link to GUI via HM-10 |
| USART6 | PC6 (TX), PC7 (RX) | 115200 | Inter-board link to DB |

## Daughterboard

| UART | Pins | Baud | Role |
|------|------|------|------|
| USART2 | PA2 (TX), PA3 (RX) | 115200 | Inter-board link to MB |

---

# 6. API

## Motherboard Hub (`hub.h`)

| Function | Description |
|----------|-------------|
| `void hub_begin(void)` | Arms USART1 for byte-by-byte interrupt reception. Must be called once after `MX_USART1_UART_Init()`. |
| `void hub_process(void)` | Dispatches any pending command. Call from the main loop; returns immediately if no command is ready. |
| `void hub_on_rx_byte(void)` | ISR hook invoked from `HAL_UART_RxCpltCallback()`. Not called directly by application code. |

## Daughterboard Hub (`db_hub.h`)

| Function | Description |
|----------|-------------|
| `void hub_begin(void)` | Arms USART2 for byte-by-byte interrupt reception. Must be called once after `MX_USART2_UART_Init()`. |
| `void hub_process(void)` | Dispatches any pending command. Call from the DB main loop. |
| `void hub_on_rx_byte(void)` | ISR hook invoked from `HAL_UART_RxCpltCallback()` on USART2. |

---

# 7. Internal State

Both hubs share the same volatile-flag / main-loop-snapshot pattern. These variables are not exposed in the public API.

| Variable | Scope | Description |
|----------|-------|-------------|
| `hub_cmd_ready` | `volatile uint8_t` | Set by ISR when a valid command frame is received |
| `hub_cmd_mask` | `volatile uint32_t` | 24-bit sensor bitmask decoded from the last valid command |
| `hub_ack_received` | `volatile uint8_t` | Set by ISR when a `0xFF` ACK byte arrives *(MB only)* |
| `hub_frame_buf[CMD_LEN]` | `uint8_t` | Accumulates incoming bytes until a complete 5-byte frame is assembled |
| `hub_resp_buf[128]` | `uint8_t` | Output buffer for the assembled sensor payload |

---

# 8. Macros

| Macro | Value | Description |
|-------|-------|-------------|
| `ACK_TIMEOUT_MS` | `2000` | Milliseconds the MB waits for a GUI ACK before retrying |
| `MAX_RETRIES` | `3` | Maximum retransmission attempts per response |
| `CAMERA1_BIT` | `1` | Bitmask bit position for Camera 1 |
| `CAMERA2_BIT` | `2` | Bitmask bit position for Camera 2 |

---

# 9. Code Flow

### MB Hub — Receive Path (ISR)

1. `HAL_UART_RxCpltCallback` fires on USART1; calls `hub_on_rx_byte()`.
2. A standalone `0xFF` byte is recognised as an ACK; `hub_ack_received` is set and the frame buffer is reset.
3. Any `0xAC` byte resets the frame buffer to allow re-synchronisation after line noise.
4. Bytes accumulate in `hub_frame_buf` until 5 bytes have arrived.
5. On a complete frame, the CRC (`hub_frame_buf[4]`) is verified against `calc_crc(hub_frame_buf, 4)`.
6. If valid, the 24-bit mask is stored in `hub_cmd_mask` and `hub_cmd_ready` is set.
7. `HAL_UART_Receive_IT` is re-armed for the next byte.

### MB Hub — Dispatch Path (Main Loop)

1. `hub_process()` checks `hub_cmd_ready`; returns immediately if clear.
2. Interrupts are briefly disabled to take a consistent snapshot of `hub_cmd_mask` and clear both flags.
3. **Camera path**: if `CAMERA1_BIT` or `CAMERA2_BIT` is set, `send_jpeg_image()` is called with retry/ACK logic. Sensor collection is skipped.
4. **Sensor path**: `build_sensor_payload(mask, hub_resp_buf)` fills MB sensor data. `db_forward(mask, out)` forwards the same mask to the DB over USART6 and appends the returned DB payload. The combined buffer is sent to the GUI with `send_with_retry()`.

### DB Hub — Receive and Dispatch Path

The DB hub mirrors the MB receive path, using USART2 instead of USART1. No ACK detection is needed (the DB is never the retry initiator). On a valid command:

1. `build_sensor_payload(mask, hub_resp_buf)` fills DB sensor data.
2. `send_fragments()` transmits the response directly to the MB over USART2.
3. The DB does not wait for ACK; the MB handles retries.

---

# 10. Main Execution Loop

Both boards follow the same startup and loop structure:

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* Peripheral initialisation (CubeMX-generated) */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    /* ... other peripherals ... */

    /* Application initialisation */
    sensors_init();   /* calls init() for every sensor in REGISTRY[] */
    hub_begin();      /* arms UART RX interrupt */

    while (1)
    {
        hub_process();  /* dispatch any pending command */
    }
}
```

`hub_process()` is the only call in the main loop. All sensor reads, inter-board forwarding, fragmentation, and retransmission occur inside that function on demand, triggered by the interrupt-set `hub_cmd_ready` flag.

**Important**: `sensors_init()` must be called after all `MX_*_Init()` functions. HAL-dependent drivers (I2C, SPI, ADC) initialise lazily via their `init()` callbacks, which require the HAL to be fully up.

---

# 11. Sensor Registry Integration

The hub does not know which sensors exist. It delegates sensor collection entirely to the registry API:

| Function | Header | Description |
|----------|--------|-------------|
| `void sensors_init(void)` | `sensor_registry.h` | Calls `init()` on every `Sensor` in `REGISTRY[]` |
| `uint16_t build_sensor_payload(uint32_t mask, uint8_t *buf)` | `sensor_registry.h` | Iterates `REGISTRY[]`; for each sensor whose `bit` is set in `mask`, calls `read()` and appends `size` bytes to `buf`. Returns bytes written. |

The `Sensor` struct and the `REGISTRY[]` array are the only integration points. Adding a new sensor requires no changes to the hub or the main loop.

### Bitmask Layout

| Bit | Board | Sensor |
|-----|-------|--------|
| 0 | MB | UV / ALS (LTR390) |
| 1 | MB | Camera 1 (OV2640) |
| 2 | MB | Camera 2 (OV2640) |
| 3 | MB | GPS |
| 4 | MB | Tilt (IIS2ICLX) |
| 5 | MB | Battery Monitor (INA219) |
| 6 | MB | EMI (lock-in) |
| 7 | MB | Humidity / Temperature (SHT45) |
| 11 | DB | Humidity / Temperature (SHT45) |
| 12 | DB | Thermistors ×4 (PS103J2) |
| 13 | DB | Turbidity |
| 14 | DB | IMU (MPU6050) |

---

# 12. Dependencies

- **STM32 HAL library**
- **UART peripherals**: USART1 (MB BLE link), USART6 (MB inter-board), USART2 (DB inter-board)
- `protocol.h` / `protocol.c` (MB) — fragmentation and CRC
- `db_protocol.h` / `db_protocol.c` (DB) — fragmentation and CRC
- `sensor_registry.h` / `sensor_registry.cpp` (MB) — sensor dispatch
- `db_sensor_registry.h` / `db_sensor_registry.cpp` (DB) — sensor dispatch
- `camera.h` (MB) — camera capture path

---

# 13. Notes

- **ISR / main-loop safety**: `hub_cmd_ready`, `hub_cmd_mask`, and `hub_ack_received` are `volatile` and are read with `__disable_irq()` / `__enable_irq()` to ensure a consistent snapshot. Never read these directly from application code.
- **Camera and sensor paths are mutually exclusive**: if any camera bit is set in the mask, sensor collection is skipped entirely in that command cycle. The GUI issues separate requests for camera and sensor data.
- **DB hub sends no ACK**: the DB relies on the MB to detect missing responses and re-issue the command. If the DB is silent (e.g. `len == 0`), the MB's `HAL_UART_Receive` will time out after 500 ms and `db_forward` will return 0.
- **CubeMX regeneration pitfall**: `main.cpp` is renamed from `main.c` to support C++. CubeMX does not update this file on regeneration. All calls to `sensors_init()` and `hub_begin()` must remain inside `USER CODE` blocks to survive a CubeMX regeneration.
- **`HAL_MAX_DELAY` must not be used** in UART polling calls inside the hub. All `HAL_UART_Receive` and `HAL_UART_Transmit` calls use finite timeouts (100–500 ms) to prevent the main loop from blocking indefinitely on a disconnected peripheral.
- **Iridium Migration**: the hub's reactive command-response model must be replaced with autonomous scheduled transmission. The polling loop becomes timer driven building the payload directly without waiting for an incoming command. Each session will send the full sensor payload and image metadata. Camera images will only be sent on Mobile Terminated requests.
