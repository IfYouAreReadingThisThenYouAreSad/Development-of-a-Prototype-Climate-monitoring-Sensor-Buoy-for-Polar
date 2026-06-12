/*
 * jpeg_decode.c
 *
 *  Created on: 26 Feb 2026
 *      Author: thomasneedham
 */


#include "jpeg_decode.h"
#include <string.h>













	//jpeg decoding function

		//---- Jpeg to ----> greyscale, functions pointers to work with "tjpgd.h"

static size_t jpeg_input(JDEC* jd, uint8_t* buffer, size_t nbyte)
{
    JpegInformation *jpeg = (JpegInformation*)jd->device;



    size_t remaining = jpeg->size - jpeg->position;
    size_t bytes_to_give = (nbyte > remaining) ? remaining : nbyte;

    if (bytes_to_give && buffer) {
        memcpy(buffer, &jpeg->data[jpeg->position], bytes_to_give);
    }

    jpeg->position += bytes_to_give;

    return bytes_to_give;
}



static int jpeg_output(JDEC* jd, void* bitmap, JRECT* rect)
{
    JpegInformation *jpeg = (JpegInformation*)jd->device;
    uint8_t* src = (uint8_t*)bitmap;

    int w = rect->right - rect->left + 1;
    int h = rect->bottom - rect->top + 1;

    for (int y = 0; y < h; y++) {
        int destY = rect->top + y;
        if (destY >= jpeg->height) break;

        for (int x = 0; x < w; x++) {
            int destX = rect->left + x;
            if (destX >= jpeg->width) break;

            jpeg->grey_image_buffer[destY * jpeg->width + destX] =
                src[y * w + x];
        }
    }

    return 1;
}




JpegStatus jpeg_decode(JpegInformation *jpeg)
{
    JDEC decoder;
    JRESULT result;

    jpeg->position = 0;   // reset read position

    result = jd_prepare(&decoder,
                        jpeg_input,
                        jpeg->work_buffer,
                        4096,               // work buffer size
                        jpeg);

    if (result != JDR_OK)
        return JPEG_ERROR_INIT;

    jpeg->width  = decoder.width;
    jpeg->height = decoder.height;

    result = jd_decomp(&decoder, jpeg_output, 0);



    if (result != JDR_OK){
        if(jpeg->size == jpeg->position && result == JDR_INP) return JPEG_OK;

        return JPEG_ERROR_DECODE;}

    return JPEG_OK;
}
