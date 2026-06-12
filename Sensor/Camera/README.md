![Platform](https://img.shields.io/badge/platform-STM32-blue)
![Language](https://img.shields.io/badge/language-C-green)
![IDE](https://img.shields.io/badge/IDE-STM32CubeIDE-orange)

# Camera Driver – Arducam OV2640 (STM32)

---

# 1. About

This directory contains a **C driver for the Arducam 2MP OV2640 camera module**, designed to run on **STM32 microcontrollers**.

The driver was developed as part of the **Prototype Climate Monitoring Sensor Buoy project**, where the system periodically captures images to monitor environmental conditions.

The implementation supports:

- OV2640 sensor configuration via **I2C**
- Image capture using the **Arducam FIFO**
- Image transfer via **SPI**
- **JPEG decoding** using **TJpgDec**
- Basic **image quality analysis** (brightness, variance, saturation)

---

# 2. Hardware

This project uses the **Arducam Mini 2MP SPI Camera Module with OV2640 sensor**.

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Camera/ArducamPhoto.jpg"
    alt="Arducam camera module" width="400">
</p>

<p align="center">Figure 1: Arducam Mini 2MP OV2640 Camera Module</p>

**Product link:**  
https://thepihut.com/products/mini-2mp-spi-camera-module-for-raspberry-pi-pico

The module contains:

- **OV2640 CMOS image sensor**
- **Arducam FIFO image buffer**
- **SPI interface for image transfer**
- **I2C interface for sensor configuration**

---

# 3. Tested Hardware

This driver has been tested with:

- **STM32F401RCT6**
- **Arducam Mini 2MP SPI Camera (OV2640)**

---

# 4. Development Environment

| Tool | Version |
|-----|--------|
| IDE | STM32CubeIDE |
| Version | 2.0.0 |
| Build | 26820_20251114_1348 |
| Vendor | STMicroelectronics |
| MCU | STM32F401RCT6 |
| Firmware Library | STM32 HAL |

---

# 5. Hardware Connections

The Arducam module communicates with the STM32 using **SPI for image data** and **I2C for camera configuration**.

## STM32 ↔ Arducam Connections

| Arducam Pin | STM32 Peripheral | Description |
|-------------|------------------|-------------|
| VCC | 3.3V | Power supply |
| GND | GND | Ground |
| SCL | I2C_SCL | I2C clock used to configure OV2640 registers |
| SDA | I2C_SDA | I2C data line used for sensor configuration |
| MOSI | SPI_MOSI | SPI data from MCU to Arducam |
| MISO | SPI_MISO | SPI data from Arducam FIFO to MCU |
| SCK | SPI_SCK | SPI clock |
| CS | GPIO | Chip select for Arducam SPI interface |

---

## Example STM32F401 Pin Mapping

| STM32 Pin | Peripheral | Function |
|-----------|-----------|----------|
| PB3 | SPI1_SCK | SPI clock |
| PB4 | SPI1_MISO | SPI data from camera |
| PB5 | SPI1_MOSI | SPI data to camera |
| PB6 | I2C1_SCL | I2C clock |
| PB7 | I2C1_SDA | I2C data |
| PB9 | GPIO Output | Arducam Chip Select |

---

# 6. API

## Functions

| Header | Function | Description |
|------|------|-------------|
| `ArducamOV2640.h` | `upload_ov2640_settings(const CameraTypeDef *camera_pins, const SensorReg *set_resolution)` | Resets the OV2640 and uploads initialization, JPEG, and resolution configuration settings |
| `ArducamOV2640.h` | `CameraStatus capture(const CameraTypeDef *camera_pins)` | Captures a JPEG image from the OV2640 sensor |
| `ArducamOV2640.h` | `CameraStatus burst_read(const CameraTypeDef *camera_pins, uint8_t *image_buffer)` | Reads JPEG bytes from the Arducam FIFO buffer over SPI |
| `ArducamOV2640.h` | `uint32_t true_length_of_jpeg(const CameraTypeDef *camera_pins, const uint8_t *image_buffer)` | Calculates the true JPEG size by detecting JPEG start/end markers |
| `jpeg_decode.h` | `JpegStatus jpeg_decode(JpegInformation *jpeg)` | Decodes JPEG data into a grayscale image using TJpgDec |
| `image_quality.h` | `ImageStatus image_processing(const JpegInformation *jpeg, ImageQualityInformation *image)` | Calculates image metrics and evaluates image quality |

---

# 7. Data Types (Structs)

## `CameraTypeDef`

Defined in **ArducamOV2640.h**

| Type | Field | Description |
|-----|-----|-------------|
| `GPIO_TypeDef *` | `port` | GPIO port used for SPI chip select |
| `uint16_t` | `pin` | GPIO pin used for SPI chip select |
| `I2C_HandleTypeDef *` | `i2c` | I2C interface connected to the OV2640 sensor |
| `SPI_HandleTypeDef *` | `spi` | SPI interface connected to the Arducam FIFO |

---

## `JpegInformation`

Defined in **jpeg_decode.h**

| Type | Field | Description |
|-----|-----|-------------|
| `uint16_t` | `width` | Image width used during decoding |
| `uint16_t` | `height` | Image height used during decoding |
| `uint8_t` | `grey_image_buffer[RESOLUTION_WIDTH * RESOLUTION_HEIGHT]` | Output grayscale image buffer |
| `uint8_t` | `work_buffer[4096]` | Temporary buffer used internally by TJpgDec |
| `uint8_t` | `data[MAX_IMAGE_SIZE]` | Raw JPEG image buffer |
| `uint32_t` | `size` | Size of the JPEG image |
| `size_t` | `position` | Current read position used by the decoder |

---

## `ImageQualityInformation`

Defined in **image_quality.h**

| Type | Field | Description |
|-----|-----|-------------|
| `uint32_t` | `saturation_low_val` | Number of underexposed pixels |
| `uint32_t` | `saturation_high_val` | Number of overexposed pixels |
| `float` | `variance_val` | Image variance (texture/spread) |
| `ImageStatus` | `suitability_val` | Result of image quality evaluation |
| `uint8_t` | `mean_val` | Average brightness of the image |

---

# 8. Macros

| Header | Macro | Description |
|------|------|-------------|
| `jpeg_decode.h` | `MAX_IMAGE_SIZE` | Maximum size of the JPEG image buffer |
| `jpeg_decode.h` | `RESOLUTION_WIDTH` | Image width used for decoding |
| `jpeg_decode.h` | `RESOLUTION_HEIGHT` | Image height used for decoding |
| `image_quality.h` | `MEAN_UPPER_THRESHOLD` | Maximum brightness threshold |
| `image_quality.h` | `MEAN_LOWER_THRESHOLD` | Minimum brightness threshold |
| `image_quality.h` | `VARIANCE_THRESHOLD` | Minimum variance threshold |
| `image_quality.h` | `MAX_SATURATED_HIGH_RATIO` | Maximum allowed percentage of overexposed pixels |
| `image_quality.h` | `MAX_SATURATED_LOW_RATIO` | Maximum allowed percentage of underexposed pixels |
| `image_quality.h` | `PIXEL_OVEREXPOSURE_LIMIT` | Pixel brightness value considered overexposed |
| `image_quality.h` | `PIXEL_UNDEREXPOSURE_LIMIT` | Pixel brightness value considered underexposed |

---

# 9. Supported Resolutions

The OV2640 sensor supports multiple JPEG resolutions.

For this project the firmware is configured for **QQVGA (160×120)** due to the **limited RAM available on the STM32F401RCT6**.

Using higher resolutions requires increasing several buffers in the code:

- `grey_image_buffer`
- `data` (JPEG storage)
- `MAX_IMAGE_SIZE`
- `RESOLUTION_WIDTH`
- `RESOLUTION_HEIGHT`

| Header | Macro | Resolution | Dimensions |
|------|------|-----------|-----------|
| `ov2640_regsV2.h` | `OV2640_160x120_JPEG` | QQVGA | 160 × 120 |
| `ov2640_regsV2.h` | `OV2640_QVGA` | QVGA | 320 × 240 |
| `ov2640_regsV2.h` | `OV2640_640x480_JPEG` | VGA | 640 × 480 |
| `ov2640_regsV2.h` | `OV2640_800x600_JPEG` | SVGA | 800 × 600 |
| `ov2640_regsV2.h` | `OV2640_1024x768_JPEG` | XGA | 1024 × 768 |
| `ov2640_regsV2.h` | `OV2640_1280x1024_JPEG` | SXGA | 1280 × 1024 |
| `ov2640_regsV2.h` | `OV2640_1600x1200_JPEG` | UXGA | 1600 × 1200 |

---

# 10. Memory Usage

Typical RAM usage when operating at **QQVGA (160×120)** resolution:

| Component | RAM Usage |
|----------|-----------|
| JPEG buffer (`data`) | ~8–20 KB |
| Grayscale buffer (`grey_image_buffer`) | 19.2 KB |
| TJpgDec working buffer | 4 KB |
| Image quality structure | ~32 bytes |
| Camera driver state | < 64 bytes |

**Approximate total RAM usage:**  
`~32–45 KB`  

Higher resolutions require increasing:

- `MAX_IMAGE_SIZE`
- `RESOLUTION_WIDTH`
- `RESOLUTION_HEIGHT`
- `grey_image_buffer`

---

# 11. Performance

Typical processing times measured on **STM32F401RCT6**:

| Stage | Approximate Time |
|------|----------------|
| Camera capture | 150 – 300 ms |
| SPI image transfer | 50 – 150 ms |
| JPEG decoding | 200 – 400 ms |
| Image quality processing | < 5 ms |

**Total processing time:** ~400 – 800 ms per captured image  

---

# 12. Enums

## `CameraStatus`

| Value | Description |
|------|-------------|
| `CAMERA_OK` | Operation successful |
| `CAMERA_ERROR_I2C` | I2C communication failure |
| `CAMERA_ERROR_I2C_RESET` | Failed to upload reset configuration |
| `CAMERA_ERROR_I2C_JPEG_INT` | Failed to upload JPEG initialization settings |
| `CAMERA_ERROR_I2C_JPEG` | Failed to upload JPEG compression settings |
| `CAMERA_ERROR_I2C_RESOLUTIONS` | Failed to upload resolution configuration |
| `CAMERA_ERROR_SPI` | SPI communication error |
| `CAMERA_ERROR_TIMEOUT` | Operation timeout |

## `ImageStatus`

| Value | Description |
|------|-------------|
| `IMAGE_OK` | Image suitable for use |
| `IMAGE_BRIGHTNESS_HIGH` | Image too bright |
| `IMAGE_BRIGHTNESS_LOW` | Image too dark |
| `IMAGE_TEXTURE_LOW` | Insufficient variance / texture |
| `IMAGE_SATURATION_HIGH` | Too many overexposed pixels |
| `IMAGE_SATURATION_LOW` | Too many underexposed pixels |

## `JpegStatus`

| Value | Description |
|------|-------------|
| `JPEG_OK` | JPEG successfully decoded |
| `JPEG_ERROR_INIT` | JPEG decoder initialization failed |
| `JPEG_ERROR_DECODE` | General decode error |

---

# 13. Code Flow

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Camera/CameraFlowDiagram.jpeg"
    alt="Camera processing flow" width="800">
</p>

<p align="center">Figure 2: Camera processing pipeline</p>

### Processing Pipeline

1. Configure the OV2640 sensor via **I2C**
2. Trigger image capture
3. Read JPEG data from the **Arducam FIFO via SPI**
4. Detect the **true JPEG length**
5. Decode the JPEG image to **grayscale**
6. Perform **image quality analysis**

---

# 14. Dependencies

- **STM32 HAL library**
- **TJpgDec** lightweight JPEG decoder
- **SPI and I2C peripherals**

---

# 15. Example Usage

```c
CameraTypeDef camera_pins = {
    .port = GPIOB,
    .pin = GPIO_PIN_9,
    .i2c = &hi2c1,
    .spi = &hspi1
};

JpegInformation jpeg;
ImageQualityInformation image_quality;

upload_ov2640_settings(&camera_pins, OV2640_160x120_JPEG);

capture(&camera_pins);

burst_read(&camera_pins, jpeg.data);

jpeg.size = true_length_of_jpeg(&camera_pins, jpeg.data);

jpeg_decode(&jpeg);

image_processing(&jpeg, &image_quality);
