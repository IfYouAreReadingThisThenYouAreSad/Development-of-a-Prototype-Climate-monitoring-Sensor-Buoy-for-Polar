/*
 * image_quality.c
 *
 *  Created on: 26 Feb 2026
 *      Author: thomasneedham
 */


#include "image_quality.h"










//-----------------FUNCTIONS---------------------//





	//---- functions that process the greyscale image



static uint8_t Mean(const Jpeg_Information *Jpeg){

	uint32_t x = 0;


	for(uint32_t n = 0; n < Jpeg->size; n++) x+= Jpeg->data[n];

	uint8_t x_bar = x/Jpeg->size;

	return x_bar;





}

static float Variance(const Jpeg_Information *Jpeg, const uint8_t mean){

	int32_t sum = 0;



	for(uint32_t i = 0; i < Jpeg->size; i++){

		int32_t diff = (int32_t)Jpeg->data[i] - (int32_t)mean;
		sum += diff*diff;



	}


	float Var = (float)sum/(float)Jpeg->size;
	return Var;
}

static uint32_t SaturationHigh(const Jpeg_Information *Jpeg){

	uint32_t saturationHigh = 0;

	for(uint32_t i = 0; i < Jpeg->size; i++){
		if(Jpeg->data[i] > PIXEL_OVEREXPOSURE_LIMIT) saturationHigh++;
	}
	return saturationHigh;
}


static uint32_t SaturationLow(const Jpeg_Information *Jpeg){

	uint32_t saturationLow = 0;

	for(uint32_t i = 0; i < Jpeg->size; i++){
		if(Jpeg->data[i] < PIXEL_UNDEREXPOSURE_LIMIT) saturationLow++;
	}
	return saturationLow;
}

static image_status_t ImageSuitable(
		const uint8_t mean,
		const float Var,
		const uint32_t saturationLow,
		const uint32_t saturationHigh,
		const uint32_t size
		){

//-----Brightness exposure
	if(mean > MEAN_UPPER_THRESHOLD) return IMAGE_BRIGHTNESS_HIGH; // image is too exposed to light, return 0 e.g not suitable
	if(mean < MEAN_LOWER_THRESHOLD) return IMAGE_BRIGHTNESS_LOW; // image is too dark, not suitable

//----- Texture in the image

	if(Var < VARIANCE_THRESHOLD) return IMAGE_TEXTURE_LOW; // not enough variation in pixels thus, image is probably usesless.

//------ are too many pixel's spoiled
	if (saturationHigh > size * MAX_SATURATED_HIGH_RATIO) return IMAGE_SATURATION_HIGH;

	if (saturationLow > size * MAX_SATURATED_LOW_RATIO) return IMAGE_SATURATION_LOW;

	return IMAGE_OK; // image is good


}


image_status_t ImageProcessing(
		const Jpeg_Information *Jpeg,
		image_quality_information *ImageInfo
		){



	ImageInfo->mean = Mean(Jpeg);

	ImageInfo->variance = Variance(Jpeg,ImageInfo->mean);

	ImageInfo->saturationHigh = SaturationHigh(Jpeg);

	ImageInfo->saturationLow = SaturationLow(Jpeg);




	ImageInfo->suitability = ImageSuitable(
			ImageInfo->mean,
			ImageInfo->variance,
			ImageInfo->saturationLow,
			ImageInfo->saturationHigh,
			Jpeg->size
			);




	return ImageInfo->suitability;










}





