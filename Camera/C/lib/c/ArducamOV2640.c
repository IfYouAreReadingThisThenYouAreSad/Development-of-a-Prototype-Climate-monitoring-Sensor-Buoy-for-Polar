/*
 * ArducamOV2640.c
 *
 *  Created on: 24 Feb 2026
 *      Author: thomasneedham
 *
 *      ov2640_regsV2.h was adpoted from https://github.com/ArduCAM/Arduino/blob/master/ArduCAM/ov2640_regs.h
 *      but was edited to fit my needs and to be able to be used in HAL
 */



//-----------------Includes-------------//

#include "ArducamOV2640.h" //<------ import function declartions and tpgd.h and tjpgdcnf for jpeg decoding
#include "jpeg_decode.h" //<------ importing Jpeg_Information struct and MAX_IMAGE_SIZE definition









//---------------- DEFINES-------------------//

	// OV260 Defines

#define OV2640_ADDR  (0x30 << 1)

	// Arducam SPI Fifo Defines

#define ARDUCHIP_FIFO 0x04
#define FIFO_CLEAR_MASK 0x01
#define FIFO_START_MASK 0x02
#define FIFO_RDPTR_RST_MASK 0x10 //
#define ARDUCHIP_MODE 0x02
#define BURST_FIFO_READ 0x3c
#define SINGLE_FIFO_READ 0x3d
#define FIFO_SIZE1				0x42  //Camera write FIFO size[7:0] for burst to read
#define FIFO_SIZE2				0x43  //Camera write FIFO size[15:8]
#define FIFO_SIZE3				0x44  //Camera write FIFO size[18:16]


#define ARDUCHIP_TRIG      		0x41  //Trigger source
#define VSYNC_MASK         		0x01
#define SHUTTER_MASK       		0x02
#define CAP_DONE_MASK      		0x08











//--------------------------defining the functions-----------------------------------------//




	//-----I2C commincations to the OV2640 Sensor on Arducam

static CameraStatus write_to_ov2650(const CameraTypeDef *camera_pins, uint8_t reg, uint8_t val){




	//set a hal type def to test later if the write worked
	HAL_StatusTypeDef i2c_status =
			HAL_I2C_Mem_Write(
					camera_pins->i2c,
					OV2640_ADDR,
					reg,
					I2C_MEMADD_SIZE_8BIT,
					&val,
					1,
					HAL_MAX_DELAY);

	if(HAL_OK == i2c_status) return CAMERA_OK;

	return CAMERA_ERROR_I2C; //unsuccessful




}

static CameraStatus uploading(const CameraTypeDef *camera_pins, const SensorReg* set_val){

	uint8_t index = 0;
	uint8_t success = 0;

	for(; !(set_val[index].reg == 0xff && set_val[index].val == 0xff); index++){ //stops if hits fake reg (e.g stop reg noted 0xff)

		success += write_to_ov2650(
				camera_pins,
				set_val[index].reg,
				set_val[index].val
				);

	}


	if(success == index) return CAMERA_OK;
	return CAMERA_ERROR_I2C;

}


CameraStatus upload_ov2640_settings(
		const CameraTypeDef *camera_pins,
		const SensorReg *set_resolution
		){

	if(!uploading(camera_pins, OV2640_reset)){ return CAMERA_ERROR_I2C;}


	if(!uploading(camera_pins, OV2640_JPEG_INIT)){ return CAMERA_ERROR_I2C;}



	if(!uploading(camera_pins, OV2640_JPEG)){return CAMERA_ERROR_I2C;}


	if(!uploading(camera_pins, set_resolution)){ return CAMERA_ERROR_I2C;}



	return CAMERA_OK;





}



	//--------SPI communication to Fifo Buffer on Arducam



static void cs_high(const CameraTypeDef *camera_pins){

	HAL_GPIO_WritePin(camera_pins->port,camera_pins->pin,GPIO_PIN_SET);
}





static void cs_low(const CameraTypeDef *camera_pins){

	HAL_GPIO_WritePin(camera_pins->port,camera_pins->pin,GPIO_PIN_RESET);

}


static CameraStatus arducam_write(const CameraTypeDef *camera_pins, uint8_t reg, uint8_t val){



	uint8_t send_data[2];
	send_data[0] = reg | 0x80; // bit masking
	send_data[1] = val;


	cs_low(camera_pins);//set SPI fifo to listening mode

//-----------send registor information---------------///

	HAL_StatusTypeDef spi_status = HAL_SPI_Transmit(camera_pins->spi, send_data, 2, HAL_MAX_DELAY);
	if(HAL_OK == spi_status){ //testing if transmit worked
		cs_high(camera_pins);
		return CAMERA_OK;
	}

	cs_high(camera_pins);// set SPI fifo to ignoring mode
	return CAMERA_ERROR_SPI;



}


static uint8_t arducam_read(const CameraTypeDef *camera_pins, uint8_t reg)
{
	//Hal function expects arrays
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = reg & 0x7F;  // MSB = 0 → READ
    tx[1] = 0x00;        // dummy byte e.g with rx[1]

    cs_low(camera_pins);

    //----start transmitting and recieving registor data-------//

    HAL_SPI_TransmitReceive(camera_pins->spi, tx, rx, 2, HAL_MAX_DELAY);

    cs_high(camera_pins);

    return rx[1];  // second byte is the register value
}

static CameraStatus flush_fifo(const CameraTypeDef *camera_pins){

	return arducam_write(camera_pins, ARDUCHIP_FIFO, FIFO_CLEAR_MASK);

}


static CameraStatus clear_fifo_flag(const CameraTypeDef *camera_pins){

	return arducam_write(camera_pins, ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}


CameraStatus capture(const CameraTypeDef *camera_pins){

//----- clear the fifo buffer flags --------//
	flush_fifo(camera_pins);

	clear_fifo_flag(camera_pins);



	HAL_Delay(5);


//--- start capturing -----//
	arducam_write(camera_pins, ARDUCHIP_FIFO, FIFO_START_MASK); // this line tells Arudcam to capture


	HAL_Delay(5);






	while (!(arducam_read(camera_pins, ARDUCHIP_TRIG) & CAP_DONE_MASK)){

		//loop makes sure Arudcam is done capturing


	}

	return CAMERA_OK;
}




uint32_t read_fifo_length(const CameraTypeDef *Camera_pins)
	{

		uint32_t len1,len2,len3,length=0;
		len1 = arducam_read(Camera_pins, FIFO_SIZE1);
	  len2 = arducam_read(Camera_pins, FIFO_SIZE2);
	  len3 = arducam_read(Camera_pins, FIFO_SIZE3) & 0x7f;
	  length = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;

		return length;
	}






static uint8_t read_fifo(const CameraTypeDef *camera_pins)
{

	return arducam_read(camera_pins, SINGLE_FIFO_READ);

}



CameraStatus burst_read(const CameraTypeDef *camera_pins, uint8_t* image_buf)
{

    uint32_t image_length = read_fifo_length(camera_pins);


    // making sure there is no memory overload //
    if (image_length == 0 || image_length > MAX_IMAGE_SIZE)
    {
        //CS_High();
        //return 0;
		image_length = MAX_IMAGE_SIZE;
    }
	if(image_length == 0){

		return CAMERA_ERROR_SPI;
	}

//first sends a command to burst reads//
    uint8_t cmd = BURST_FIFO_READ;
    cs_low(camera_pins);
    HAL_SPI_Transmit( camera_pins->spi, &cmd, 1, HAL_MAX_DELAY);

// then reads fifo//
    for(uint32_t i = 0; i < image_length; i++)
    {
        uint8_t dummy = 0x00;
        HAL_SPI_TransmitReceive(camera_pins->spi, &dummy, &image_buf[i], 1, HAL_MAX_DELAY);
    }

    cs_high(camera_pins);
    return CAMERA_OK;}



uint32_t true_length_of_jpeg(const uint8_t *image_buffer){


	uint32_t length = 0;

	for(
			;
			!(image_buffer[length] == 0xff && image_buffer[length+1] == 0xd9); //jpegs ends with 0xff and then 0xd9
			length++
			){
		if(MAX_IMAGE_SIZE == length) return MAX_IMAGE_SIZE; //again protecting from memory overload
	}

	length++;


	return length;


}







