# Camera Driver – Arducam OV2640 (STM32)

## 1. About

This directory contains a **C driver for the Arducam 2MP OV2640 camera module**, designed to run on **STM32 microcontrollers**.

The driver was developed as part of the **Prototype Climate Monitoring Sensor Buoy project**, where the system requires periodic image acquisition to monitor environmental conditions.

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
    alt="Figure 1 shows the Arducam camera module" width="400">
</p>
<p align="center">Figure 1: Arducam Mini 2MP OV2640 Camera Module</p>

**Product link**

https://thepihut.com/products/mini-2mp-spi-camera-module-for-raspberry-pi-pico

The module contains:

- **OV2640 CMOS image sensor**
- **Arducam FIFO buffer**
- **SPI interface for image data**
- **I2C interface for sensor configuration**

---

# 3. API

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

# 4. Data Types (Structs)

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

# 5. Macros

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

# 6. Enums

## `CameraStatus`

Used for debugging and error reporting during camera configuration and image capture.

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

---

## `ImageStatus`

Returned by `image_processing()` to indicate whether the captured image is suitable for transmission.

| Value | Description |
|------|-------------|
| `IMAGE_OK` | Image suitable for use |
| `IMAGE_BRIGHTNESS_HIGH` | Image too bright |
| `IMAGE_BRIGHTNESS_LOW` | Image too dark |
| `IMAGE_TEXTURE_LOW` | Insufficient variance / texture |
| `IMAGE_SATURATION_HIGH` | Too many overexposed pixels |
| `IMAGE_SATURATION_LOW` | Too many underexposed pixels |

---

## `JpegStatus`

Returned by `jpeg_decode()`.

| Value | Description |
|------|-------------|
| `JPEG_OK` | JPEG successfully decoded |
| `JPEG_ERROR_INIT` | JPEG decoder initialization failed |
| `JPEG_ERROR_DECODE` | General decode error |

---

# 7. Code Flow

For detailed information about individual functions, refer to the **header files**, which contain full documentation describing parameters and return values.

An example implementation is provided in the **Example folder**, using an **STM32F401RCT6** with the **HAL driver library**.

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Camera/CameraFlowDiagram.jpeg"
    alt="Figure 2 shows the camera processing flow" width="1000">
</p>
<p align="center">Figure 2: Camera processing pipeline</p>

### Processing Pipeline

1. Configure the OV2640 sensor over **I2C**
2. Trigger image capture
3. Read JPEG data from the **Arducam FIFO via SPI**
4. Detect the **true JPEG length**
5. Decode the JPEG image to **grayscale**
6. Perform **image quality analysis**

---

# 8. Dependencies

This driver requires:

- **STM32 HAL library**
- **TJpgDec** lightweight JPEG decoder
- **SPI and I2C peripherals**

---

# 9. Example Usage

Typical capture pipeline:

```c

  CameraTypeDef camera_pins ={
	  .port = GPIOB,  //cs port
	  .pin = GPIO_PIN_9,    //cs pin

	  .i2c = &hi2c1,

	  .spi = &hspi1


  };
capture(&camera);

burst_read(&camera, jpeg.data);

jpeg.size = true_length_of_jpeg(&camera, jpeg.data);

jpeg_decode(&jpeg);

image_processing(&jpeg, &image_quality);
```

---

# 10. Notes

- The driver is designed for **low-power embedded systems**
- JPEG decoding uses **grayscale output** to reduce RAM usage
- Image quality filtering prevents **useless images from being transmitted**
- Threshold values in `image_quality.h` can be tuned depending on the environment

