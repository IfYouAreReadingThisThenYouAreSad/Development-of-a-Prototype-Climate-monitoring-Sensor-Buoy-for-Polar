# 1.0 About

This directory contains code for the Arducam 2MP OV2640 in C on the STM32. Which was needed to incorparte image acquistion on the buoy project.

# 2.0 Part information

For this we used the Arducam that can take pictues in various resolutions

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Camera/ArducamPhoto.jpg"
    alt="Figure 1 shows the GPS code structure" width="400">
</p>
<p align="center">Figure 1 shows the Arducam</p>

Here is where you can obtain the Arducam:
++https://thepihut.com/products/mini-2mp-spi-camera-module-for-raspberry-pi-pico


# 3.0 API

##Functions

| .h | Function | Description |
|----|----------|-------------|
| ArducamOV2640.h | `upload_ov2640_settings(const CameraTypeDef *camera_pins, const SensorReg *set_resolution)` | Resets, Initializes image configuartion and resolution|
| ArducamOV2640.h | `CameraStatus capture(const CameraTypeDef *camera_pins)` | Captures a JPEG image from the OV2640 |
| ArducamOV2640.h | `CameraStatus burst_read(const CameraTypeDef *camera_pins, uint8_t* image_buffer)` | Reads Jpeg bytes from Arducam fifo buffer |
| ArducamOV2640.h | `uint32_t true_length_of_jpeg(const CameraTypeDef *camera_pins, const uint8_t *image_buffer);` | gets the true length of jpeg image |
| jpeg_decode.h | `JpegStatus jpeg_decode(JpegInformation *jpeg)` | Decodes JPEG image data to grey scales using TJpgDec |
| image_quality.h | `ImageStatus image_processing(const JpegInformation *jpeg,ImageQualityInformation *image);` | Calculates greyscale image metrics and compares them to thresholds |



## Data Types (Struts)

### `CameraTypeDef` (ArducamOV2640.h)

| Type | Field  Description |
|------|------|-------------|
| `GPIO_TypeDef *` | `port` | GPIO port used for CS enable for SPI |
| `uint16_t` | `pin` | GPIO pin used to CS enable for SPI |
| `I2C_HandleTypeDef *` | `i2c` | I2C interface connected to OV2640 chip on Arducam |
| `SPI_HandleTypeDef *`| `spi`  | SPI interface connected to Fifo buffer on Arducam  |


### `JpegInformation` (jpeg_decode.h)

| Type | Field  Description |
|------|------|-------------|
| `uint16_t` | `width` | width used for jpeg_decode function |
| `uint16_t` | `height` | height used for jpeg_deoce function |
| `uint8_t` | `grey_image_buffer[ RESOLUTION_WIDTH * RESOLUTION_HEIGHT ]` | arrary where grey scale image will be stored |
| `uint8_t` | `work_buffer[4096]` | working buffer to store byte os data buffer to uses within a function pointer in jpeg_decode, do not use |
| `uint8_t` | `data[MAX_IMAGE_SIZE]` | arrary that stores the jpeg image |
| `uint32_t` | `size` | Size of the Jpeg |
| `size_t` | `position` | Used in jpeg_decode withing a function pointer |


### `ImageQualityInformation` (image_quality.h)

| Type | Field  Description |
|------|------|-------------|
| `uint32_t` | `saturation_low_val` | width used for jpeg_decode function |
| `uint32_t` | `saturation_high_val` | height used for jpeg_deoce function |
| `float` | `variance_val` | arrary where grey scale image will be stored |
| `ImageStatus` | `suitability_val` | working buffer to store byte os data buffer to uses within a function pointer in jpeg_decode, do not use |
| `uint8_t` | `mean_val` | arrary that stores the jpeg image |


## Macros

| .h | Macro | Description |
|----|-------|-------------|
| jpeg_decode.h | `MAX_IMAGE_SIZE` | sets maximum size of the jpeg (memory managment) |
| jpeg_decode.h | `RESOLUTION_WIDTH` | sets resolution width, used to make size of grey_image_buffer arrary in JpegInformation struct |
| jpeg_decode.h | `RESOLUTION_HEIGHT` | sets resolution height, used to make size of grey_image_buffer arrary in JpegInformation struct |
| image_quality.h | `MEAN_UPPER_THRESHOLD` | Brightness threshold e.g if goes above this value then image too bright to use. |
| image_quality.h | `MEAN_LOWER_THRESHOLD` | Darkness threshold e.g if goes below this values then image is too dark to use. |
| image_quality.h | `VARIANCE_THRESHOLD` | Texture threshold/Spread threshold. |
| image_quality.h | `MAX_SATURATED_HIGH_RATIO` | Percentage of the total number of pixels that has to be contaminated with too high brightness for the whole image to be considered not use-able. |
| image_quality.h | `MAX_SATURATED_LOW_RATIO` | Percentage of the total number of pixels that has to be contaminated with too dark for the image to be considered not use-able. |
| image_quality.h | `PIXEL_OVEREXPOSURE_LIMIT` | Pixel is too bright to be used, if pixel is above this limit then it will increment a counter in SaturationHigh(..) |
| image_quality.h | `PIXEL_UNDEREXPOSURE_LIMIT` | Pixel is too dark to be used, if pixel is below this limit then it will increment a counter in SaturationHigh(..) |




# 4.0 Code Flow
For in depthh information of the functions look at the .h file which is commented and tell the user what the function does and what 
parameters its take and what it returns. There is an example folder which was done STM32F401RCT6, which was coded in HAL..

The code follows a simple sturcture which does not need more explaining then the figure below:

<p align="center">
  <img src="[https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/GPS_Module/GPS%20Flow%20diagam.jpeg](https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Camera/CameraFlowDiagram.jpeg)"
    alt="Figure 2 shows the Arducam code structure" width="400">
</p>
<p align="center">Figure 2 shows the Arducam code structure</p>





