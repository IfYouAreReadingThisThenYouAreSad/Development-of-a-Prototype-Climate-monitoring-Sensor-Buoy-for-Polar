/*
 * jpeg_decode.h
 *
 *  Created on: 26 Feb 2026
 *      Author: thomasneedham
 *
 *
 *      This .h piggies back off the functions in tjpgd.h and tjpgdcnf for turn jpeg to grey-scale
 *      thanks to sago35 for making this. check it out in his git repo:
 *
 *      https://github.com/sago35/go-tjpgd
 *
 *
 */

#ifndef INC_JPEG_DECODE_H_
#define INC_JPEG_DECODE_H_

#ifdef __cplusplus
extern "C" {
#endif


//----------------------INCLUDES-----------------------//

#include <stdint.h>
//including jpeg decoding lib e.g library will be used to turn jpeg into grey-scaled image
#include "tjpgd.h"
#include "tjpgdcnf.h"




//---------------- DEFINES-------------------//


	// Jpeg/image max size defines

#define MAX_IMAGE_SIZE 10000 //<----- feel free to change but I set this to 10k to stop ram issues on my tiny MCU



#define RESOLUTION_WIDTH 160 //<---- change this with resolutin im using 160x120 hence why 160

#define RESOLUTION_HEIGHT 120//<---- change this with resolution



//-------jpeg storage----///

typedef struct  {

      // actual length of JPEG in bytes
	uint16_t width;
	uint16_t height;
	uint8_t grey_image_buffer[ RESOLUTION_WIDTH * RESOLUTION_HEIGHT ];
	uint8_t work_buffer[4096]; // must be big enough for IDCT work
	uint8_t data[MAX_IMAGE_SIZE];  // big enough for your JPEG

	uint32_t size;
	size_t position;





}JpegInformation;




//------- ENUM's--------------//

typedef enum {
    JPEG_OK = 0,
    JPEG_ERROR_INIT,
    JPEG_ERROR_DECODE
} JpegStatus;






				//FUNCTIONS

//------------jpeg decoding fuctions function----------------//

	//---- Jpeg to ----> greyscale, functions pointers to work with "tjpgd.h"





/**
 * @brief  Decode a JPEG image into a grayscale pixel buffer.
 *
 * This function uses the Tiny JPEG Decompressor (TJpgDec) library to
 * decode a compressed JPEG image stored in the provided Jpeg_Information
 * structure. The decoded grayscale pixel data is written into the
 * greyimageBuffer member of the structure.
 *
 * The function internally prepares the decoder, binds the required
 * input/output callbacks, and performs full image decompression.
 *
 * @param[in,out] jpeg  Pointer to a Jpeg_Information structure containing:
 *                      - data: Pointer to the compressed JPEG data
 *                      - size: Length of the JPEG data in bytes
 *                      - workBuffer: Working buffer required by TJpgDec
 *                      - greyimageBuffer: Destination buffer for decoded image
 *
 *                      On successful return, the structure is updated with:
 *                      - width:  Decoded image width in pixels
 *                      - height: Decoded image height in pixels
 *
 * @retval JPEG_OK             JPEG decoded successfully
 * @retval JPEG_ERROR_INIT     Decoder initialization failed
 * @retval JPEG_ERROR_DECODE   JPEG decompression failed
 *
 * @note  The workBuffer must be sufficiently large for the configured
 *        TJpgDec settings (typically 4 KB or greater).
 *
 * @warning The greyimageBuffer must be large enough to store
 *          (width × height) bytes before calling this function.
 */
JpegStatus jpeg_decode(JpegInformation *jpeg);








#ifdef __cplusplus
}
#endif





#endif /* INC_JPEG_DECODE_H_ */
