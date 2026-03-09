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

static uint8_t WriteOV2640(const camera_pin_assigment *Camera_pins, uint8_t reg, uint8_t val){




	//set a hal type def to test later if the write worked
	HAL_StatusTypeDef I2C_status =
			HAL_I2C_Mem_Write(
					Camera_pins->i2c,
					OV2640_ADDR,
					reg,
					I2C_MEMADD_SIZE_8BIT,
					&val,
					1,
					HAL_MAX_DELAY);


	if(HAL_OK == I2C_status) return 1;

	return 0; //unsuccessful




}

static camera_status_t Uploading(const camera_pin_assigment *Camera_pins, const sensor_reg* Set_Val){

	uint8_t index = 0;
	uint8_t Success = 0;

	for(; !(Set_Val[index].reg == 0xff && Set_Val[index].val == 0xff); index++){ //stops if hits fake reg (e.g stop reg noted 0xff)

		Success += WriteOV2640(
				Camera_pins,
				Set_Val[index].reg,
				Set_Val[index].val
				);

	}


	if(Success == index) return CAMERA_OK;
	return CAMERA_ERROR_I2C;

}


camera_status_t Upload_OV2640_Settings(
		const camera_pin_assigment *Camera_pins,
		const sensor_reg *set_resolution
		){

	if(Uploading(Camera_pins, OV2640_reset) != CAMERA_OK ){ return CAMERA_ERROR_I2C_RESET;}



	if(Uploading(Camera_pins, OV2640_JPEG_INIT) != CAMERA_OK){ return CAMERA_ERROR_I2C_JPEG_INT;}



	if(Uploading(Camera_pins, OV2640_JPEG) != CAMERA_OK){return CAMERA_ERROR_I2C_JPEG;}


	if(Uploading(Camera_pins, set_resolution) != CAMERA_OK){ return CAMERA_ERROR_I2C_RESOLUTIONS;}



	return CAMERA_OK;





}



	//--------SPI communication to Fifo Buffer on Arducam



static void CS_High(const camera_pin_assigment *Camera_pins){

	HAL_GPIO_WritePin(Camera_pins->port,Camera_pins->pin,GPIO_PIN_SET);
}





static void CS_Low(const camera_pin_assigment *Camera_pins){

	HAL_GPIO_WritePin(Camera_pins->port,Camera_pins->pin,GPIO_PIN_RESET);

}


static camera_status_t ArducamWrite(const camera_pin_assigment *Camera_pins, uint8_t reg, uint8_t val){



	uint8_t sendData[2];
	sendData[0] = reg | 0x80; // bit masking
	sendData[1] = val;


	CS_Low(Camera_pins);//set SPI fifo to listening mode

//-----------send registor information---------------///

	HAL_StatusTypeDef SPI_status = HAL_SPI_Transmit(Camera_pins->spi, sendData, 2, HAL_MAX_DELAY);
	if(HAL_OK == SPI_status){ //testing if transmit worked
		CS_High(Camera_pins);
		return CAMERA_OK;
	}

	CS_High(Camera_pins);// set SPI fifo to ignoring mode
	return CAMERA_ERROR_SPI;



}


static uint8_t ArducamRead(const camera_pin_assigment *Camera_pins, uint8_t reg)
{
	//Hal function expects arrays
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = reg & 0x7F;  // MSB = 0 → READ
    tx[1] = 0x00;        // dummy byte e.g with rx[1]

    CS_Low(Camera_pins);

    //----start transmitting and recieving registor data-------//

    HAL_SPI_TransmitReceive(Camera_pins->spi, tx, rx, 2, HAL_MAX_DELAY);

    CS_High(Camera_pins);

    return rx[1];  // second byte is the register value
}

static camera_status_t flush_fifo(const camera_pin_assigment *Camera_pins){

	return ArducamWrite(Camera_pins, ARDUCHIP_FIFO, FIFO_CLEAR_MASK);

}


static camera_status_t clear_fifo_flag(const camera_pin_assigment *Camera_pins){

	return ArducamWrite(Camera_pins, ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}


camera_status_t Capture(const camera_pin_assigment *Camera_pins){

//----- clear the fifo buffer flags --------//
	flush_fifo(Camera_pins);

	clear_fifo_flag(Camera_pins);



	HAL_Delay(5);


//--- start capturing -----//
	ArducamWrite(Camera_pins, ARDUCHIP_FIFO, FIFO_START_MASK); // this line tells Arudcam to capture


	HAL_Delay(5);






	while (!(ArducamRead(Camera_pins, ARDUCHIP_TRIG) & CAP_DONE_MASK)){

		//loop makes sure Arudcam is done capturing


	}

	return CAMERA_OK;
}




uint32_t Read_Fifo_Length(const camera_pin_assigment *Camera_pins)
	{

		uint32_t len1,len2,len3,length=0;
		len1 = ArducamRead(Camera_pins, FIFO_SIZE1);
	  len2 = ArducamRead(Camera_pins, FIFO_SIZE2);
	  len3 = ArducamRead(Camera_pins, FIFO_SIZE3) & 0x7f;
	  length = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;

		return length;
	}






static uint8_t Read_Fifo(const camera_pin_assigment *Camera_pins)
{

	return ArducamRead(Camera_pins, SINGLE_FIFO_READ);

}



camera_status_t Burst_Read(const camera_pin_assigment *Camera_pins, uint8_t* imageBuf)
{

    uint32_t imagelength = Read_Fifo_Length(Camera_pins);


    // making sure there is no memory overload //
    if (imagelength == 0 || imagelength > MAX_IMAGE_SIZE)
    {
        //CS_High();
        //return 0;
		imagelength = MAX_IMAGE_SIZE;
    }
	if(imagelength == 0){

		return CAMERA_ERROR_SPI;
	}

//first sends a command to burst reads//
    uint8_t cmd = BURST_FIFO_READ;
    CS_Low(Camera_pins);
    HAL_SPI_Transmit(Camera_pins->spi, &cmd, 1, HAL_MAX_DELAY);

// then reads fifo//
    for(uint32_t i = 0; i < imagelength; i++)
    {
        uint8_t dummy = 0x00;
        HAL_SPI_TransmitReceive(Camera_pins->spi, &dummy, &imageBuf[i], 1, HAL_MAX_DELAY);
    }

    CS_High(Camera_pins);
    return CAMERA_OK;}



uint32_t True_Length_Of_Jpeg(const camera_pin_assigment *Camera_pins, const uint8_t *imageBuffer){

	uint32_t imagelength = Read_Fifo_Length(Camera_pins);
	uint32_t length = 0;
	uint32_t endsignature = 0;

	for(
			;
			length < imagelength; //jpegs ends with 0xff and then 0xd9
			length++
			){
		if(imageBuffer[length] == 0xff && imageBuffer[length+1] == 0xd9) endsignature = length;

		if(MAX_IMAGE_SIZE == length) return MAX_IMAGE_SIZE; //again protecting from memory overload
	}

	endsignature++;


	return endsignature;


}
