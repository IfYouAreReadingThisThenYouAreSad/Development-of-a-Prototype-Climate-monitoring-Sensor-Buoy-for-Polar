# BLE / HM-10 Transport Layer

# 1. About

This directory contains the **wire protocol** used to transmit sensor data and camera imagery between the **Motherboard (MB)** and the **host ground-station GUI**.

During development and field trials, a **HM-10 BLE module** provides the physical link. The HM-10 operates as a transparent UART-to-BLE bridge: the STM32 firmware writes bytes to USART1 and the module forwards them over Bluetooth Low Energy to the host PC, where the Python GUI receives them via the `bleak` library. No BLE-specific commands are issued by the firmware; from the STM32's perspective the link is a plain serial port.

The protocol is deliberately designed to match the **340-byte SBD message limit of the Iridium 9603N** satellite modem that will replace the HM-10 for polar deployment. Each command and each fragment fits within that constraint, so the firmware transport layer can be substituted at compile time without changing the application logic.

The protocol layer consists of two files:

- `protocol.h` / `protocol.c` — framing, CRC, and fragment transmission for the **MB → GUI** link (USART1)
- `db_protocol.h` / `db_protocol.c` — an identical framing layer for the **MB → DB** inter-board link (USART6/USART2); see [Section 10](#10-inter-board-link-differences) for differences

---

# 2. Part Information

The BLE module used is the **HM-10**, a BLE 4.0 module with a transparent UART interface.

| Property | Value |
|----------|-------|
| Interface | UART (transparent serial) |
| Baud rate | 9600 |
| BLE standard | Bluetooth 4.0 LE |
| Default role | Peripheral (connectable) |
| Default name | `HMSoft` |

The HM-10 requires no firmware-level BLE commands. It is configured once over AT commands at power-on and then acts as a wire.
---

# 3. Tested Hardware

This driver has been tested with:

- **STM32F405RGT6**
- **HM-10 BLE module**
- **Python GUI** 

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

# 5. Hardware Connections

The HM-10 communicates with the STM32 Motherboard over a **UART interface**.

## STM32 MB ↔ HM-10 Connections

| HM-10 Pin | STM32 Peripheral | Description |
|-----------|-----------------|-------------|
| VCC | 3.3V | Power supply |
| GND | GND | Ground |
| TX | USART1_RX (PA10) | HM-10 transmits to STM32 |
| RX | USART1_TX (PA9) | STM32 transmits to HM-10 |

## STM32 MB Pin Mapping

| STM32 Pin | Peripheral | Function |
|-----------|-----------|----------|
| PA9 | USART1_TX | Sends fragments and ACKs to GUI via HM-10 |
| PA10 | USART1_RX | Receives commands and ACKs from GUI via HM-10 |

**UART settings:** 9600 baud

---

# 6. API

The protocol layer exposes two functions, both declared in `protocol.h`:

| Header | Function | Description |
|--------|----------|-------------|
| `protocol.h` | `uint8_t calc_crc(const uint8_t *buf, uint8_t len)` | Checksum over `len` bytes of `buf`. Used to validate commands and authenticate fragments. |
| `protocol.h` | `void send_fragments(const uint8_t *buf, uint32_t len, uint8_t stx_more, uint8_t stx_last)` | Splits `buf` into 16-byte payload fragments and transmits each as a 20-byte packet over USART1. The last fragment uses `stx_last`; all others use `stx_more`. |

The application layer (`hub.cpp`) calls `send_fragments` directly. The protocol layer has no knowledge of sensors, cameras, or the hub state machine.

---

# 7. Frame Definitions

## Command Frame (GUI → MB)

Sent by the host to request data. 5 bytes total.

| Byte | Field | Value |
|------|-------|-------|
| 0 | STX | `0xAC` |
| 1 | `MASK_L` | Bits 0–7 of the 24-bit sensor bitmask |
| 2 | `MASK_M` | Bits 8–15 of the 24-bit sensor bitmask |
| 3 | `MASK_H` | Bits 16–23 of the 24-bit sensor bitmask |
| 4 | CRC | Checksum of bytes 0–3 |

## Sensor Fragment (MB → GUI)

Each response is split into one or more 20-byte fragments.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | STX | `0xAA` (more to follow) or `0xAF` (last fragment) |
| 1 | IDX_H | High byte of fragment index |
| 2 | IDX_L | Low byte of fragment index |
| 3–18 | Payload | Up to 16 bytes of sensor data |
| 19 | CRC | Checksum of bytes 0–18 |

The last fragment is zero-padded if the total payload length is not a multiple of 16.

## Image Fragment (MB → GUI)

| Byte | Field | Description |
|------|-------|-------------|
| 0 | STX | `0xA0` (more bytes), `0xA1` (last bytes), `0xA2` (metadata more), `0xA3` (metadata last) |
| 1 | IDX_H | High byte of fragment index |
| 2 | IDX_L | Low byte of fragment index |
| 3–18 | Payload | 16 bytes of JPEG data or image quality metadata |
| 19 | CRC | XOR of bytes 0–18 |

## ACK Byte (GUI → MB)

A single byte `0xFF` sent by the GUI to acknowledge receipt of a complete response. The MB retries transmission up to `MAX_RETRIES` times if no ACK arrives within `ACK_TIMEOUT_MS`.

---

# 8. Macros

| Header | Macro | Value | Description |
|--------|-------|-------|-------------|
| `protocol.h` | `STX_SENSOR_MORE` | `0xAA` | Sensor fragment, more to follow |
| `protocol.h` | `STX_SENSOR_LAST` | `0xAF` | Sensor fragment, final |
| `protocol.h` | `STX_IMAGE_MORE` | `0xA0` | Image fragment, more to follow |
| `protocol.h` | `STX_IMAGE_LAST` | `0xA1` | Image fragment, final |
| `protocol.h` | `STX_IMAGE_META_MORE` | `0xA2` | Image quality metadata fragment, more to follow |
| `protocol.h` | `STX_IMAGE_META_LAST` | `0xA3` | Image quality metadata fragment, final |
| `protocol.h` | `STX_CMD` | `0xAC` | Start-of-command byte |
| `protocol.h` | `STX_ACK` | `0xFF` | ACK byte |
| `protocol.h` | `CMD_LEN` | `5` | Command frame length in bytes |
| `protocol.h` | `FRAG_SIZE` | `20` | Fragment frame length in bytes |
| `protocol.h` | `PAYLOAD_BYTES` | `16` | Payload bytes per fragment |

---

# 9. Code Flow

### Outbound (MB → GUI)

1. Hub builds the sensor or image payload in a local buffer.
2. `send_fragments()` divides the buffer into 16-byte chunks.
3. Each chunk is wrapped in a 20-byte frame: `[STX][IDX_H][IDX_L][16 bytes payload][CRC]`.
4. Frames are transmitted over USART1 with a 20 ms inter-frame delay to allow the HM-10 BLE module time between packets.
5. After all fragments are sent, the MB waits up to `ACK_TIMEOUT_MS` (2000 ms) for a `0xFF` ACK from the GUI. Up to `MAX_RETRIES` (3) retransmission attempts are made.

### Inbound (GUI → MB)

1. USART1 is armed for byte-by-byte interrupt reception via `HAL_UART_Receive_IT`.
2. Each byte is appended to a 5-byte frame buffer.
3. On a complete frame, the CRC is verified. A valid frame sets the `hub_cmd_ready` flag and stores the decoded 24-bit bitmask.
4. A standalone `0xFF` byte is recognised as an ACK and sets `hub_ack_received` without disturbing the command framing state.

---

# 10. Host Interface

The host-side counterpart is `ble_manager.py` in the Python ground-station GUI. It uses the `bleak` library to connect to the HM-10's BLE GATT characteristic and exchanges bytes using the same frame format defined in this document.

From the host's perspective:

- **Sending a command**: write a 5-byte command frame to the GATT TX characteristic.
- **Receiving a response**: collect 20-byte notification frames until a `STX_SENSOR_LAST` or `STX_IMAGE_LAST` STX byte is received. Send `0xFF` to the TX characteristic to ACK.
- **Sensor payload layout**: bytes arrive in the order that sensors appear in `REGISTRY[]` in `sensor_registry.cpp`. The host must know the `size` of each requested sensor to parse the concatenated payload.

See `ble_manager.py` for the complete host-side implementation.

---

# 11. Inter-Board Link Differences

The `db_protocol.h` / `db_protocol.c` files implement the same CRC and fragmentation logic for the **MB ↔ DB UART link** (USART6 on the MB, USART2 on the DB). Two differences apply:

1. **UART handle**: `db_protocol.c` transmits on `huart2` (DB inter-board link) rather than `huart1` (BLE to GUI).
2. **Last-fragment IDX_L encoding**: On the DB link, the `IDX_L` byte of the final fragment is repurposed to carry the number of valid payload bytes in that fragment. The MB's `db_forward()` reads `frag[2]` as the byte count on the last fragment. This eliminates the need for the MB to maintain a sensor-size lookup table for DB sensors.
3. **No inter-frame delay**: The 20 ms `HAL_Delay` present in `protocol.c` is omitted from `db_protocol.c`. The direct UART connection between boards does not require the pacing that the HM-10 BLE module needs.

---

# 12. Dependencies

- **STM32 HAL library**
- **UART peripheral** (USART1 at 9600 baud for the BLE link)

---

# 13. Example Usage

```c
#include "protocol.h"

/* Transmit a 20-byte sensor payload as fragments */
uint8_t payload[20] = { /* sensor bytes */ };
send_fragments(payload, sizeof(payload), STX_SENSOR_MORE, STX_SENSOR_LAST);

/* Compute and verify a CRC */
uint8_t frame[5] = { 0xAC, 0x01, 0x00, 0x00, 0x00 };
frame[4] = calc_crc(frame, 4);
```

---

# 14. Notes

- The 9600 baud rate is dictated by the HM-10's default UART configuration. Higher baud rates are possible by AT command but have not been tested.
- The 20 ms inter-frame delay in `protocol.c` is required to prevent the HM-10 BLE module from dropping bytes at 9600 baud. Remove or reduce this delay only after testing throughput at the target rate.
- Each 20-byte fragment fits within the Iridium 9603N SBD 340-byte message limit. The BLE → Iridium transport substitution therefore requires changes only to the physical layer (`huart1` handle and baud rate), not to the framing logic.
- Camera JPEG data is too large for reliable Iridium SBD transmission. Further tests are needed to verify the image transmission. Scalar sensor data and image quality metadata are suitable for satellite uplink.
