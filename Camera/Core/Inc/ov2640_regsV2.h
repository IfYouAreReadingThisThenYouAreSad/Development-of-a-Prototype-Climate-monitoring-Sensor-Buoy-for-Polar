/*
 * ov2640_regsV2.h
 *
 *  Created on: 13 Feb 2026
 *      Author: thomasneedham
 *
 */

#ifndef INC_OV2640_REGSV2_H_
#define INC_OV2640_REGSV2_H_

#define OV2640_CHIPID_HIGH 	0x0a
#define OV2640_CHIPID_LOW 	0x0b

#include <stdint.h>


typedef struct {
	uint8_t reg;
	uint8_t val;
}sensor_reg;



extern const sensor_reg OV2640_QVGA[];
extern const sensor_reg OV2640_JPEG_INIT[];
extern const sensor_reg OV2640_YUV422[];
extern const sensor_reg OV2640_JPEG[];
extern const sensor_reg OV2640_160x120_JPEG[];
extern const sensor_reg OV2640_176x144_JPEG[];
extern const sensor_reg OV2640_320x240_JPEG[];
extern const sensor_reg OV2640_352x288_JPEG[];
extern const sensor_reg OV2640_640x480_JPEG[];
extern const sensor_reg OV2640_800x600_JPEG[];
extern const sensor_reg OV2640_1024x768_JPEG[];
extern const sensor_reg OV2640_1280x1024_JPEG[];
extern const sensor_reg OV2640_1600x1200_JPEG[];



#endif /* INC_OV2640_REGSV2_H_ */
