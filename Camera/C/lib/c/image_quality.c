/*
 * image_quality.c
 *
 *  Created on: 26 Feb 2026
 *      Author: thomasneedham
 */


#include "image_quality.h"










//-----------------FUNCTIONS---------------------//





	//---- functions that process the greyscale image



static uint8_t mean(const JpegInformation *jpeg){

	uint32_t x = 0;


	for(uint32_t n = 0; n < jpeg->size; n++) x+= jpeg->grey_image_buffer[n];

	uint8_t x_bar = x/jpeg->size;

	return x_bar;





}

static float variance(const JpegInformation *jpeg, const uint8_t mean){

	int32_t sum = 0;



	for(uint32_t i = 0; i < jpeg->size; i++){

		int32_t diff = (int32_t)jpeg->grey_image_buffer[i] - (int32_t)mean;
		sum += diff*diff;



	}


	float var = (float)sum/(float)jpeg->size;
	return var;
}

static uint32_t saturation_high(const JpegInformation *jpeg){

	uint32_t saturation_high_temp = 0;

	for(uint32_t i = 0; i < jpeg->size; i++){
		if(jpeg->grey_image_buffer[i] > PIXEL_OVEREXPOSURE_LIMIT) saturation_high_temp++;
	}
	return saturation_high_temp;
}


static uint32_t saturation_low(const JpegInformation *jpeg){

	uint32_t saturation_low_temp = 0;

	for(uint32_t i = 0; i < jpeg->size; i++){
		if(jpeg->grey_image_buffer[i] < PIXEL_UNDEREXPOSURE_LIMIT) saturation_low_temp++;
	}
	return saturation_low_temp;
}

static ImageStatus image_suitable(
		const uint8_t mean,
		const float variance,
		const uint32_t saturation_low_temp,
		const uint32_t saturation_high_temp,
		const uint32_t size
		){

//-----Brightness exposure
	if(mean > MEAN_UPPER_THRESHOLD) return IMAGE_BRIGHTNESS_HIGH; // image is too exposed to light, return 0 e.g not suitable
	if(mean < MEAN_LOWER_THRESHOLD) return IMAGE_BRIGHTNESS_LOW; // image is too dark, not suitable

//----- Texture in the image

	if(variance < VARIANCE_THRESHOLD) return IMAGE_TEXTURE_LOW; // not enough variation in pixels thus, image is probably usesless.

//------ are too many pixel's spoiled
	if (saturation_high_temp > size * MAX_SATURATED_HIGH_RATIO) return IMAGE_SATURATION_HIGH;

	if (saturation_low_temp > size * MAX_SATURATED_LOW_RATIO) return IMAGE_SATURATION_LOW;

	return IMAGE_OK; // image is good


}


ImageStatus image_processing(
		const JpegInformation *jpeg,
		ImageQualityInformation *image
		){



	image->mean_val = mean(jpeg);

	image->variance_val = variance(jpeg,image->mean_val);

	image->saturation_high_val = saturation_high(jpeg);

	image->saturation_low_val = saturation_low(jpeg);




	image->suitability_val = image_suitable(
			image->mean_val,
			image->variance_val,
			image->saturation_low_val,
			image->saturation_high_val,
			jpeg->size
			);




	return image->suitability_val;










}
