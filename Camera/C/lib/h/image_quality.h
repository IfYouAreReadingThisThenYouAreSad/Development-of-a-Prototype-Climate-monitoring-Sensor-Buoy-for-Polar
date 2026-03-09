/*
 * image_quality.h
 *
 *  Created on: 26 Feb 2026
 *      Author: thomasneedham
 */

#ifndef INC_IMAGE_QUALITY_H_
#define INC_IMAGE_QUALITY_H_



#ifdef __cplusplus
extern "C" {
#endif


//----------------------INCLUDES-----------------------//

#include "stm32f4xx_hal.h"
#include "jpeg_decode.h"






//--------------------ENUM's--------------------------//

	//-----used as message for image processing --------//


typedef enum {
	IMAGE_OK = 0,
	IMAGE_BRIGHTNESS_HIGH,
	IMAGE_BRIGHTNESS_LOW,
	IMAGE_TEXTURE_LOW,
	IMAGE_SATURATION_HIGH,
	IMAGE_SATURATION_LOW
} image_status_t;







//----storage of image quality metrics------//

typedef struct {

	uint32_t saturationLow;
	uint32_t saturationHigh;
	float variance;
	image_status_t suitability;
	uint8_t mean;




}image_quality_information;






//----------------DEFINES---------------------//


/* feel free to change these values, these values deterime if image is fit for uses,
 * at the moment they are set to anartic conditions (and my dorm room)
 * All defines that below are used in the ImageSuitable(...) function
 * which compares image to below thresholds.
 */



#define MEAN_UPPER_THRESHOLD 240 /*<----- Brightness threshold e.g if goes above this value
										  then image too bright to use. */

#define MEAN_LOWER_THRESHOLD 20  /*<----- Darkness threshold e.g if goes below this values
										  then image is too dark to use. */


#define VARIANCE_THRESHOLD 3000  /*<----- Texture threshold e.g if below this value then their is
										  not enough variation in terrian or just one thing is caputured
										  like snow (that would have a low variance) but if snow and
										  rocks was captured this would have a high variance and would
										  pass. */

#define MAX_SATURATED_HIGH_RATIO 0.30f  /*<---- Percentage of the total number of pixels that has to be
												contaminated with too high brightness for the whole
												image to be considered not use-able. e.g if 35% of
												pixels in the image are too bright , then
												image is not suitable */


#define MAX_SATURATED_LOW_RATIO 0.35f /*<---- Percentage of the total number of pixels that has to be
												contaminated with too dark for the image to be
												considered not use-able. e.g if 35% of pixels in the
												image are too dark , then image not suitable. */




/* define below is used in SaturationHigh(...) to control if indivual pixel brightness
 * are too bright for uses.
 */


#define PIXEL_OVEREXPOSURE_LIMIT 240 /*<---- Pixel is too bright to be used, if pixel is above this
									         limit then it will increment a counter in SaturationHigh(..)
									         */




/* define below is used in SaturationLOW(...) to control if indivual pixel brightness
 * are to0 dark for uses.
 */




#define PIXEL_UNDEREXPOSURE_LIMIT 6 /*<---- Pixel is too dark to be used, if pixel is below this
									        limit then it will increment a counter in SaturationHigh(..)
									        */







/**
 * @brief function is an abstraction that calculates the mean, varaince, how many pixel are saturated
 * too high (over-exposed), how many pixels are saturated low (under exposed). the function then takes
 * all these matrics and applies them to thresholds and determines if the image is suitable.
 * The function will also store all these metrics such as the mean , variance etc is a struct passed to
 * it
 *
 *
 * @param arrary where grey-scale image is stored
 *
 * @param size of grey-scale image (e.g its resolution)
 *
 * @param image_quality_information struct to store all the metric the function calculates
 *
 * @return IMAGE_OK if image is suitable for sending or will return an error depending on what matric it
 * fails at.
 */
image_status_t ImageProcessing(
		const Jpeg_Information *Jpeg,
		image_quality_information *ImageInfo
		);



#ifdef __cplusplus
}
#endif


#endif /* INC_IMAGE_QUALITY_H_ */
