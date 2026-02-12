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
#include "usart.h"
#include "gpio.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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

uint8_t GPS_OFF(void);
uint8_t GPS_ON(void);
uint8_t getNMEA(void);
void DecodeNMEA(void);
void paraphase(void);
uint8_t checksum(void);
uint8_t HexToDec(char hexValue);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
volatile uint8_t GPS_Buf[100];
volatile uint8_t GPS_index =0;
char* GPS_token[10];

/*char time[10];
char postition[4][10];
char data[10];
char speedKnots[10];
char course[10];

char speedKmh[10];
char altitude[10]; */

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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);


	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

	HAL_Delay(500);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  GPS_ON();
	  getNMEA();
	  if(checksum()){
		  paraphase();
		  DecodeNMEA();
	  }




		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}
	 // while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}
  /* USER CODE END 3 */
}
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
uint8_t GPS_OFF(void){

	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_RESET);

	return 0;
}
uint8_t GPS_ON(void){

	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_SET);

	return 1;
}

uint8_t getNMEA(void){

	GPS_index = 0;
	uint8_t GPS_char = '\0';
	static uint8_t buffer[100]; // buffer & index are here becuase i cannot see
	static uint8_t index = 0; // GPS_Buf or gpsIndex in debugger, hence using these for debugging

	HAL_StatusTypeDef GPS_status = HAL_UART_Receive(&huart1, &GPS_char, 1, 100);
	if(HAL_OK == GPS_status && GPS_char == '$'){
		  buffer[GPS_index] = GPS_char;
		  HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
		  GPS_Buf[GPS_index] = buffer[GPS_index];
		  GPS_index++;
		  HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
		while(GPS_char != '\n'){


			//uint8_t GPS_char;
			HAL_StatusTypeDef GPS_status = HAL_UART_Receive(&huart1, &GPS_char, 1, 100);

			  if(HAL_OK == GPS_status){
				  buffer[GPS_index] = GPS_char;
				  HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
				  GPS_Buf[GPS_index] = buffer[GPS_index];
				  GPS_index++;
				  HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
				  if (GPS_char == '\n'){
					  buffer[GPS_index] = '\0';
					  GPS_Buf[GPS_index] = buffer[GPS_index];

					  return 1;}}
			  if(GPS_index >= sizeof(buffer) -1 ){ // stops buffer overflow, resets index counter

				  GPS_index = 0;
				  return 0;;
			  }

		}}
	return 0;
}


void DecodeNMEA(void){

	char time[10];
	char postition[4][10];
	char data[10];
	char speedKnots[10];
	char course[10];

	char speedKmh[10];
	char altitude[10];  // for debugging

	//-------------------------------$GPRMC------------------------------------------//
	if(strncmp(GPS_token[0], "$GPRMC", 6) == 0 && strncmp(GPS_token[2], "A", 1) == 0){


		strncpy(time, GPS_token[1], 9);
		time[9] = '\0';
		strncpy(speedKnots, GPS_token[7], 9);
		speedKnots[9] = '\0';
		strncpy(course, GPS_token[8],9);
		course[9] = '\0';
		strncpy(data,GPS_token[9],9);
		data[9] = '\0';
		for(uint8_t i = 3, idx = 0; i < 6; i++){
			strncpy(postition[idx],GPS_token[i],9);
			postition[idx][9] = '\0';
			idx++;
		}
		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}
	}
	//-------------------------------$GPGGA------------------------------------------//
	else if(strncmp(GPS_token[0], "$GPGGA", 6) == 0 && GPS_token[6][0] != '0'){
		strncpy(time, GPS_token[1], 9);
		time[9] = '\0';
		for(uint8_t i = 2, idx = 0; i < 5; i++){
			strncpy(postition[idx],GPS_token[i],9);
			postition[idx][9] = '\0';
			idx++;
		}
		strncpy(altitude, GPS_token[9], 9);
		altitude[9] = '\0';
		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}



	}
	//-------------------------------$GPVTG------------------------------------------//
	else if(strncmp(GPS_token[0], "$GPVTG",6) == 0){

		strncpy(speedKmh,GPS_token[7], 9);
		speedKmh[9] = '\0';
		/* note can added things to record bearing angles*/
		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}
	}
	//-------------------------------$GPGSA------------------------------------------//
	else if(strncmp(GPS_token[0], "$GPGSA", 6) == 0){

		/* mainly going to be used to see if the gps should keep trying to get posiston*/
		if(GPS_token[2][0] == '0'){
			//GPS_OFF();//used to turn of gps if there is no satalines in sight
		}
		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}
	}


}


void paraphase(void){



	  char token[11][20];//using this to see into GPS_token with debugger
	  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); //debugging
	  GPS_token[0] = strtok((char*)GPS_Buf,","); //the (char*) tells the compliler to shut the fuck up
	  strncpy(token[0], GPS_token[0], 19);
	  token[0][19] = '\0';
	  for(uint8_t i =1; i < 10 ; i++){
		  GPS_token[i] = strtok(NULL,","); //this with with line 121 splits the buffer 6 times for parphasing
		  strncpy(token[i], GPS_token[i], 19);
		  token[i][19] = '\0';
		  if(GPS_token[i] == NULL) return;
	  }





}



uint8_t checksum(void){

	uint8_t idx = 1; // since buffer [0] will have $
	uint8_t checksumValue = 0;
	uint8_t GPSchecksumValue;
	while(GPS_Buf[idx] != '*' && idx <= GPS_index){ //the extra && protect from overflow


		checksumValue ^= GPS_Buf[idx++]; // doing XOR along the NMEA and then increasing idx



	}

	GPSchecksumValue = HexToDec(GPS_Buf[idx+1])*16 + HexToDec(GPS_Buf[idx+2]);


	if(checksumValue == GPSchecksumValue) return 1; // return 1 if data is not currupt

	return 0; // returns 0 if data is currupt







}

uint8_t HexToDec(char hexValue){


	switch(hexValue){


	case('F'):
			return 15;
	case('E'):
			return 14;
	case('D'):
			return 13;
	case('C'):
			return 12;
	case('B'):
			return 11;
	case('A'):
			return 10;
	case('9'):
			return 9;
	case('8'):
			return 8;
	case('7'):
			return 7;
	case('6'):
			return 6;
	case('5'):
			return 5;
	case('4'):
			return 4;
	case('3'):
			return 3;
	case('2'):
			return 2;
	case('1'):
			return 1;
	case('0'):
			return 0;


	default:
		return 16; //invalid




	}








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
