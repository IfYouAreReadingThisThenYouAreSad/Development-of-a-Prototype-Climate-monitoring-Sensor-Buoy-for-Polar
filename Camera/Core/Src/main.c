/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "ov2640_regsV2.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define OV2640_ADDR  (0x30 << 1)

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



#define MAX_IMAGE_SIZE 60000

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */



uint8_t WriteOV2640(uint8_t reg, uint8_t val);
uint8_t ReadOV2640(uint8_t reg, uint8_t* value);
uint8_t Uploading(const sensor_reg* Set_Val);
uint8_t Upload_OV2640_Settings(void);
void CS_High(void);
void CS_Low(void);
uint8_t VerifySettings(const sensor_reg* Set_Val);

uint8_t ArducamWrite(uint8_t reg, uint8_t val);
uint8_t ArducamRead(uint8_t reg);


uint8_t flush_fifo(void);
uint8_t capture(void);
uint8_t clear_fifo_flag(void);
uint32_t read_fifo_length(void);
uint8_t read_fifo(void);

uint8_t BurstRead(uint8_t* imageBuf);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t FailUpload = 0;


static   uint8_t imageBuf[MAX_IMAGE_SIZE];


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  //-------GPS settings ------//
  /*
    OV2640_JPEG_INIT
	OV2640_YUV422
	OV2640_JPEG
	OV2640_176x144_JPEG */

  CS_Low();
  if(Upload_OV2640_Settings()){

	  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	  HAL_Delay(500);
	  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
  }
  capture();


  //uint8_t imageBuf[MAX_IMAGE_SIZE];
  BurstRead(imageBuf);

HAL_Delay(500);



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	 // HAL_GPIO_WritePin(GPIOC, GPIO_PIN_C, GPIO_RESET); // onbard LED on



  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */


uint8_t WriteOV2640(uint8_t reg, uint8_t val){ // mainly used to shit a bunch of shit(seetings) onto OV2640


	HAL_StatusTypeDef I2C_status = HAL_I2C_Mem_Write(&hi2c1,OV2640_ADDR, reg,I2C_MEMADD_SIZE_8BIT, &val, 1, HAL_MAX_DELAY);

	if(HAL_OK == I2C_status) return 1;

	return 0; //unsuccessful




}


uint8_t ReadOV2640(uint8_t reg, uint8_t* val){// YEAH this doesnt fucking work

	uint8_t temp =0;



	HAL_StatusTypeDef I2C_status = HAL_I2C_Master_Transmit(&hi2c1, 0x30 << 1, &reg, 1, HAL_MAX_DELAY);
    if (I2C_status != HAL_OK) return 0; // fail

    // Step 2: read 1 byte from that register
	HAL_StatusTypeDef I2C_status2 = HAL_I2C_Master_Receive(&hi2c1, (0x30 << 1) | 0x01, val, 1, HAL_MAX_DELAY);
    if (I2C_status2 != HAL_OK) return 0; // fail

    temp = *val;

	return 1;

}


uint8_t Uploading(const sensor_reg* Set_Val){ // uploads a setting

	uint8_t index = 0;
	uint8_t Success = 0;
	uint8_t temp = 0;

	for(; !(Set_Val[index].reg == 0xff && Set_Val[index].val == 0xff); index++){ //stops if hits fake reg (e.g stop reg noted 0xff)

		Success += WriteOV2640(Set_Val[index].reg, Set_Val[index].val);
		temp = Set_Val[index].val;
	}


	if(Success == index) return 1;
	return 0;

}


uint8_t Upload_OV2640_Settings(void){ // uploads all the settings at once piggies back off Uploading()



	if(!Uploading(OV2640_JPEG_INIT)){FailUpload++; return 0;}
	if(!Uploading(OV2640_YUV422)) {FailUpload++; return 0;}
	if(!Uploading(OV2640_JPEG)) {FailUpload++; return 0;}
	if(!Uploading(OV2640_176x144_JPEG)) {FailUpload++; return 0;}

	//FailUploaded global variable, tells me which setting failed to upload, will do something later with it

	return 1;





}



void CS_High(void){

	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_SET);
}
void CS_Low(void){

	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_RESET);

}

uint8_t VerifySettings(const sensor_reg* Set_Val){ //yeah this doesnt work and i dont care

	uint8_t tempVal = 0;
	uint8_t index = 0;
	uint8_t fails = 0;
	uint8_t Success = 0;
	uint8_t temp = 0;


	while(!(Set_Val[index].reg == 0xff && Set_Val[index].val == 0xff)){


		if(!ReadOV2640(Set_Val[index].reg, &tempVal)){fails++;}
		temp = tempVal;

		if(tempVal == Set_Val[index].val)Success++;
		else fails++;

		index++;




	}



	if(Success == index)return 1;
	return 0;


}


uint8_t ArducamWrite(uint8_t reg, uint8_t val){



	uint8_t sendData[2];
	sendData[0] = reg | 0x80;
	sendData[1] = val;


	CS_Low();

	HAL_StatusTypeDef SPI_status = HAL_SPI_Transmit(&hspi1, sendData, 2, HAL_MAX_DELAY);
	if(HAL_OK == SPI_status){
		CS_High();
		return 1;
	}

	CS_High();
	return 0;



}
uint8_t ArducamRead(uint8_t reg)
{
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = reg & 0x7F;  // MSB = 0 → READ
    tx[1] = 0x00;        // dummy byte

    CS_Low();
    HAL_StatusTypeDef SPI_status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
    CS_High();

    return rx[1];  // second byte is the register value
}

uint8_t flush_fifo(void){

	return ArducamWrite(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);

}
uint8_t capture(void){


	uint8_t checker = 0;
	checker += flush_fifo();
	checker += clear_fifo_flag();
	checker += ArducamWrite(ARDUCHIP_FIFO, FIFO_START_MASK);

	while (!(ArducamRead(ARDUCHIP_TRIG) & CAP_DONE_MASK));

	return checker;

}
uint8_t clear_fifo_flag(void){

	return ArducamWrite(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

uint32_t read_fifo_length(void)
{
	uint32_t len1,len2,len3,length=0;
	len1 = ArducamRead(FIFO_SIZE1);
  len2 = ArducamRead(FIFO_SIZE2);
  len3 = ArducamRead(FIFO_SIZE3) & 0x7f;
  length = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;
	return length;
}
uint8_t read_fifo(void)
{

	return ArducamRead(SINGLE_FIFO_READ);

}
uint8_t BurstRead(uint8_t* imageBuf){

	uint32_t imagelength = read_fifo_length();
	CS_Low();

	if(imagelength > MAX_IMAGE_SIZE || imagelength == 0){
		CS_High();
		return 0; //failed to read
	}
	uint8_t Burst = BURST_FIFO_READ;

	HAL_SPI_Transmit(&hspi1, &Burst, 1, HAL_MAX_DELAY);


	HAL_SPI_Receive(&hspi1, imageBuf, imagelength, HAL_MAX_DELAY);
	CS_High();
	//if (!(imageBuf[0] == 0xFF && imageBuf[1] == 0xD8)) return 1;

	return 1;
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
