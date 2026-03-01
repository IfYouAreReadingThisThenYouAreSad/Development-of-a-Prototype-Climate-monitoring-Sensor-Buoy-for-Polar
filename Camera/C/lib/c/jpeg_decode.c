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

static size_t jpegInput(JDEC* jd, uint8_t* buff, size_t nbyte)
{
    Jpeg_Information *jpeg = (Jpeg_Information*)jd->device;

    size_t remaining = jpeg->size - jpeg->position;
    size_t bytesToGive = (nbyte > remaining) ? remaining : nbyte;

    if (bytesToGive && buff) {
        memcpy(buff, &jpeg->data[jpeg->position], bytesToGive);
    }

    jpeg->position += bytesToGive;

    return bytesToGive;
}



static int jpegOutput(JDEC* jd, void* bitmap, JRECT* rect)
{
    Jpeg_Information *jpeg = (Jpeg_Information*)jd->device;
    uint8_t* src = (uint8_t*)bitmap;

    int w = rect->right - rect->left + 1;
    int h = rect->bottom - rect->top + 1;

    for (int y = 0; y < h; y++) {
        int destY = rect->top + y;
        if (destY >= jpeg->height) break;

        for (int x = 0; x < w; x++) {
            int destX = rect->left + x;
            if (destX >= jpeg->width) break;

            jpeg->greyimageBuffer[destY * jpeg->width + destX] =
                src[y * w + x];
        }
    }

    return 1;
}




jpeg_status_t jpeg_decode(Jpeg_Information *jpeg)
{
    JDEC decoder;
    JRESULT result;

    jpeg->position = 0;   // reset read position

    result = jd_prepare(&decoder,
                        jpegInput,
                        jpeg->workBuffer,
                        4096,               // work buffer size
                        jpeg);

    if (result != JDR_OK)
        return JPEG_ERROR_INIT;

    jpeg->width  = decoder.width;
    jpeg->height = decoder.height;

    result = jd_decomp(&decoder, jpegOutput, 0);

    if (result != JDR_OK)
        return JPEG_ERROR_DECODE;

    return JPEG_OK;
}

