/*
 * ArducamOV2640.h
 *
 *  Created on: 24 Feb 2026
 *      Author: thomasneedham
 */

#ifndef INC_ARDUCAMOV2640_H_
#define INC_ARDUCAMOV2640_H_


#ifdef __cplusplus
extern "C" {
#endif

//---------INCLUDES----------//
#include "stm32f4xx_hal.h"
#include <stdint.h> // <--- to uses structs

#include "ov2640_regsV2.h" //<------ importing OV2640 strut registor defines





//----------ENUM's--------------------------//

	//----used as debugging message for functions-----///
typedef enum {
    CAMERA_OK = 0,
	CAMERA_ERROR_I2C,
    CAMERA_ERROR_I2C_RESET,
    CAMERA_ERROR_I2C_JPEG_INT,
    CAMERA_ERROR_I2C_JPEG,
    CAMERA_ERROR_I2C_RESOLUTIONS,
    CAMERA_ERROR_SPI,
    CAMERA_ERROR_TIMEOUT
} CameraStatus;






//--------------Struct-------------------//

//struct for how the user has wired up Arudcam to STM

typedef struct {


//------ CS pin ----///

GPIO_TypeDef *port;
uint16_t pin;
/* CS pin should always be set to pull high, if accidently set to pull low then other funcitons
* will not work thus either at the begging of int main pull it up high with CS_High or set it high
* is hardware.
*/


//------- I2C pins ----------//

I2C_HandleTypeDef *i2c;


//---------SPI pins ----------//
SPI_HandleTypeDef *spi;


} CameraTypeDef;










//----------------FUNCTION---------------------//


	//-----I2C commincations to the OV2640 Sensor on Arducam






/**
 * @brief uploads all setting to take Jpeg photo at once
 *
 * @param OV2640 configuration of how you wired up Arudcam
 *
 * @param resolution of picture  e.g OV2640_160x120_JPEG[], take picture with resolution of 160x120
 *
 * @return 1 if successful, 2-5 with the number indication which setting it failed to upload.
 */
CameraStatus upload_ov2640_settings(const CameraTypeDef *camera_pins, const SensorReg *set_resolution);









	//--------SPI communication to Fifo Buffer on Arducam



/**
 * @brief this function calls flush_fifo and clear_fifo_flag to essentially clear the fifo
 * 		  then sends message to start capturing and record data
 *
 * @param camrea pin struct to know who the users hooked up the spi pin to STM
 *
 * @return CAMERA_OK e.g 0 if successful if not will return an error message
 */
CameraStatus capture(const CameraTypeDef *camera_pins);


/**
 * @brief get an approximation of how much data is stored in the fifo, note this is an
 * 		  approximation , this is why i made another functin called TrueLengthOfJpeg
 *
 * @param camrea pin struct to know who the users hooked up the spi pin to STM
 *
 * @return 1 if transmit was successful, 0 if not.
 */

uint32_t read_fifo_length(const CameraTypeDef *camera_pins);


/**
 * @brief brust reads to fifo buffer
 *
 * @param camrea pin struct to know who the users hooked up the spi pin to STM
 *
 * @param users buffer delared in code for image
 *
 * @return CAMERA_OK e.g 0 if successful if not will return an error message
 */
CameraStatus burst_read(const CameraTypeDef *camera_pins, uint8_t* image_buffer);


/**
 * @brief gives the ture lenfth of the jpeg by reading the imagebuffer in code not fifo
 *
 *
 * @param users buffer delared in code
 *
 * @returns length of jpeg stored in arrary
 */
uint32_t true_length_of_jpeg(const CameraTypeDef *camera_pins, const uint8_t *image_buffer);









#ifdef __cplusplus
}
#endif





#endif /* INC_ARDUCAMOV2640_H_ */
