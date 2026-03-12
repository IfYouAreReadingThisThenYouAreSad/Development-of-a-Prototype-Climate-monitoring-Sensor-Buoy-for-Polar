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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */





 //         _____________________________ D7
 //        |  _________________________D6
 //        | |  _____________________D5
 //        | | |  __________________D4
 //        | | | |  ______________Back light
 //        | | | | |  _________enable
 //        | | | | | |  _______RW
 //        | | | | | | |  ______ RS
 //        | | | | | | | |
 //        | | | | | | | |
///     0b 1 1 1 1 1 1 1 1


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LCD_ADDRESS (0x3E << 1)
#define LCD_LIGHT_ADDRESS (0x6B << 1)





/*!
 *   LCD Command Set
 */
#define LCD_CLEARDISPLAY 0x01       // Command to clear the display
#define LCD_RETURNHOME 0x02         // Command to return the cursor to home position
#define LCD_ENTRYMODESET 0x04       // Command to set entry mode
#define LCD_DISPLAYCONTROL 0x08     // Command to control the display settings
#define LCD_CURSORSHIFT 0x10       // Command to shift cursor or display
#define LCD_FUNCTIONSET 0x20       // Command to set the function (like display mode)
#define LCD_SETCGRAMADDR 0x40      // Command to set the CGRAM address
#define LCD_SETDDRAMADDR 0x80      // Command to set the DDRAM address

/*!
 *   Entry Mode Flags for LCD
 */
#define LCD_ENTRYRIGHT 0x00        // Cursor moves right
#define LCD_ENTRYLEFT 0x02         // Cursor moves left
#define LCD_ENTRYSHIFTINCREMENT 0x01 // Shift the display to the right automatically
#define LCD_ENTRYSHIFTDECREMENT 0x00 // Shift the display to the left automatically

/*!
 *   Display Control Flags
 */
#define LCD_DISPLAYON 0x04         // Turn the display on
#define LCD_DISPLAYOFF 0x00        // Turn the display off
#define LCD_CURSORON 0x02          // Show the cursor
#define LCD_CURSOROFF 0x00         // Hide the cursor
#define LCD_BLINKON 0x01           // Blink the cursor
#define LCD_BLINKOFF 0x00          // Do not blink the cursor

/*!
 *   Cursor and Display Shift Flags
 */
#define LCD_DISPLAYMOVE 0x08       // Move the display
#define LCD_CURSORMOVE 0x00        // Move the cursor
#define LCD_MOVERIGHT 0x04         // Move the display/cursor to the right
#define LCD_MOVELEFT 0x00          // Move the display/cursor to the left

/*!
 *   Function Set Flags
 */
#define LCD_8BITMODE 0x10          // Set 8-bit mode
#define LCD_4BITMODE 0x00          // Set 4-bit mode
#define LCD_2LINE 0x08             // Use 2 lines
#define LCD_1LINE 0x00             // Use 1 line
#define LCD_5x8DOTS 0x00           // Set 5x8 dots for character display

/*!
 *   LED Device I2C Address
 */
#define SN3193_IIC_ADDRESS  0x6B   // I2C address of the SN3193 backlight controller

// SN3193 Register Definitions
#define SHUTDOWN_REG  0x00         // Register for software shutdown mode
#define BREATING_CONTROL_REG  0x01 // Register for breathing effect control
#define LED_MODE_REG  0x02        // Register for LED mode control
#define LED_NORNAL_MODE  0x00     // Normal mode for LED operation
#define LED_BREATH_MODE  0x20     // Breathing mode for LED operation

#define CURRENT_SETTING_REG  0x03 // Register to set the output current
#define PWM_1_REG  0x04          // PWM duty cycle data for channel 1
#define PWM_2_REG  0x05          // PWM duty cycle data for channel 2
#define PWM_3_REG  0x06          // PWM duty cycle data for channel 3
#define PWM_UPDATE_REG  0x07     // Register to load PWM settings

#define T0_1_REG  0x0A           // Set T0 time for OUT1
#define T0_2_REG  0x0B           // Set T0 time for OUT2
#define T0_3_REG  0x0C           // Set T0 time for OUT3

#define T1T2_1_REG  0x10         // Set T1&T2 time for OUT1
#define T1T2_2_REG  0x11         // Set T1&T2 time for OUT2
#define T1T2_3_REG  0x12         // Set T1&T2 time for OUT3

#define T3T4_1_REG  0x16         // Set T3&T4 time for OUT1
#define T3T4_2_REG  0x17         // Set T3&T4 time for OUT2
#define T3T4_3_REG  0x18         // Set T3&T4 time for OUT3

#define TIME_UPDATE_REG  0x1C    // Register to load time register data
#define LED_CONTROL_REG  0x1D    // Register to enable OUT1~OUT3 (LED outputs)
#define RESET_REG  0x2F          // Register to reset all registers to default values




typedef struct {
	uint8_t command;
	uint8_t function;
} LCD_COMMAND_STRUCT;


//---------Clear display



typedef enum{
	LCD_ERROR_I2C =0,
	LCD_OK,
	LCD_ERROR_I2C_REG,
	LCD_ERROR_FUNCTION_SET,
	LCD_ERROR_DISPLAY_ON,
	LCD_ERROR_CLEAR_SCREEN,
	LCD_ERROR_ENTRY_MODE,
	LCD_ERROR_SETTING_BRIGHTNESS,
	LCD_ERROR_SETTING_BACKLIGHT_MODE,
	LCD_ERROR_SENDING_STRING,
	LCD_ERROR_CREATE_CHAR
}LCD_STATUS;

static uint8_t _showfunction = 0;
static uint8_t _showcontrol = 0;
static uint8_t _showmode = 0;
static uint8_t _numlines = 0;  // Set the number of lines
static uint8_t _currline = 0;      // Start with the first line


LCD_STATUS lcd_i2c_write_command( uint8_t function_byte){ //used for sending commands


	uint8_t sending_buffer[2];
	sending_buffer[0] = 0x80; //0x80
	sending_buffer[1] = function_byte;

	HAL_StatusTypeDef i2c_status =
	HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDRESS, sending_buffer, 2, HAL_MAX_DELAY);

    HAL_Delay(5);  // small delay after every command

	if(i2c_status == HAL_OK) return LCD_OK;

	return LCD_ERROR_I2C;
}


LCD_STATUS lcd_i2c_write_data( uint8_t function_byte){// used for sending char


	uint8_t sending_buffer[2];
	sending_buffer[0] = 0x40;
	sending_buffer[1] = function_byte;

	HAL_StatusTypeDef i2c_status =
	HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDRESS, sending_buffer, 2, HAL_MAX_DELAY);

	if(i2c_status == HAL_OK) return LCD_OK;

	return LCD_ERROR_I2C;
}








LCD_STATUS i2c_write_backlight(uint8_t reg, uint8_t val){
    uint8_t buffer[2];
    buffer[0] = reg;
    buffer[1] = val;

    HAL_StatusTypeDef i2c_status =
        HAL_I2C_Master_Transmit(&hi2c1, LCD_LIGHT_ADDRESS, buffer, 2, HAL_MAX_DELAY);

    if(HAL_OK == i2c_status) return LCD_OK;
    return LCD_ERROR_I2C_REG;
}

LCD_STATUS backlight_init(void){


	 i2c_write_backlight(RESET_REG, 0x00);
	    HAL_Delay(10);


    if(LCD_OK != i2c_write_backlight(SHUTDOWN_REG, 0x20)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(LED_MODE_REG, LED_NORNAL_MODE)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(CURRENT_SETTING_REG, 0x00)) return LCD_ERROR_I2C;
    HAL_Delay(10);

    if(LCD_OK != i2c_write_backlight(PWM_1_REG, 0xFF)) return LCD_ERROR_I2C;

    HAL_Delay(100);
    if(LCD_OK != i2c_write_backlight(PWM_UPDATE_REG, 0x00)) return LCD_ERROR_I2C;

    if(LCD_OK != i2c_write_backlight(T0_1_REG, 0x40)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(T0_2_REG, 0x40)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(T0_3_REG, 0x40)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    if(LCD_OK != i2c_write_backlight(T1T2_1_REG, 0x26)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(T1T2_2_REG, 0x26)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(T1T2_3_REG, 0x26)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    if(LCD_OK != i2c_write_backlight(T3T4_1_REG, 0x26)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(T3T4_2_REG, 0x26)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(T3T4_3_REG, 0x26)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    if(LCD_OK != i2c_write_backlight(LED_CONTROL_REG, 0x01)) return LCD_ERROR_I2C;
    if(LCD_OK != i2c_write_backlight(TIME_UPDATE_REG, 0x00)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    return LCD_OK;
}

LCD_STATUS set_brightness(uint8_t value)
{
    // Check if the input value is within the valid range (0-100)
    if (value < 0 || value > 100)
    {
        return LCD_ERROR_SETTING_BRIGHTNESS;  // Print an error message if the value is invalid
    }
    else
    {
        // Set the PWM duty cycle based on the brightness percentage
        i2c_write_backlight(PWM_1_REG, (uint8_t)((float)value * (2.55)));  // Convert percentage to 0-255 scale
        HAL_Delay(100);  // Wait for the PWM settings to take effect
        i2c_write_backlight(PWM_UPDATE_REG, 0x00);  // Apply the PWM settings
    }

    return LCD_OK;

}

// Set the operation mode of the LEDs (e.g., breathing mode or steady mode)
LCD_STATUS set_led_mode(uint8_t mode)
{

	if(mode != 0x20 || mode != 0x00) return LCD_ERROR_SETTING_BACKLIGHT_MODE;

	return i2c_write_backlight(LED_MODE_REG, mode);  // Set the LED mode
    // Mode options: 0x20 for breathing mode, 0x00 for steady mode


}






// Turn on the display
LCD_STATUS display()
{
    _showcontrol |= LCD_DISPLAYON;  // Set the display to "on"
    return lcd_i2c_write_command(LCD_DISPLAYCONTROL | _showcontrol);  // Apply the display control settings
}

// Clear the display and reset the cursor position
LCD_STATUS clear()
{
	LCD_STATUS command_status;
	command_status = lcd_i2c_write_command(LCD_CLEARDISPLAY);  // Send the clear display command
    HAL_Delay(2);  // Wait for 2ms to allow the command to complete (this takes time)
    return command_status;
}


// Set the cursor position on the LCD display
LCD_STATUS set_cursor(uint8_t col, uint8_t row)
{
    // Depending on the row, set the appropriate base address for the column
    col = (row == 0 ? col | 0x80 : col | 0xc0);

    return lcd_i2c_write_command(col);  // Send the command to set the cursor position
}

// Write data to the LCD
LCD_STATUS data(uint8_t value)
{
 // 0x40 indicates data (not a command)
    return lcd_i2c_write_data(value);  // Send the data byte to the LCD
}

// Send a string to the LCD display
LCD_STATUS send_string(const char *str)
{

	uint8_t i = 0, success = 0;
    // Loop through each character of the string and send it to the display
    for (; str[i] != '\0'; i++){
    	if(LCD_OK == lcd_i2c_write_data(str[i])) success++;  // Send each character as data
    }

    if(i == success) return LCD_OK;

    return LCD_ERROR_SENDING_STRING;
}

// Disable the cursor (hide the cursor)
LCD_STATUS noCursor()
{
    _showcontrol &= ~LCD_CURSORON;  // Clear the LCD_CURSORON bit to disable the cursor
    return lcd_i2c_write_command(LCD_DISPLAYCONTROL | _showcontrol);  // Send the command to update the display control
}

// Enable the cursor (show the cursor)
LCD_STATUS cursor()
{
    _showcontrol |= LCD_CURSORON;  // Set the LCD_CURSORON bit to enable the cursor
    return lcd_i2c_write_command(LCD_DISPLAYCONTROL | _showcontrol);  // Send the command to update the display control
}

// Scroll the display content to the left
LCD_STATUS scrollDisplayLeft(void)
{
	return lcd_i2c_write_command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);  // Shift the display left
}

// Scroll the display content to the right
LCD_STATUS scrollDisplayRight(void)
{
	return lcd_i2c_write_command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);  // Shift the display right
}

// Set the text direction to left-to-right
LCD_STATUS leftToRight(void)
{
    _showmode |= LCD_ENTRYLEFT;  // Set the entry mode for left-to-right direction
    return lcd_i2c_write_command(LCD_ENTRYMODESET | _showmode);  // Send the command to update the entry mode
}

// Set the text direction to right-to-left
LCD_STATUS rightToLeft(void)
{
    _showmode &= ~LCD_ENTRYLEFT;  // Clear the LCD_ENTRYLEFT bit for right-to-left direction
    return lcd_i2c_write_command(LCD_ENTRYMODESET | _showmode);  // Send the command to update the entry mode
}

// Disable auto-scrolling of text
LCD_STATUS noAutoscroll(void)
{
    _showmode &= ~LCD_ENTRYSHIFTINCREMENT;  // Clear the LCD_ENTRYSHIFTINCREMENT bit
    return lcd_i2c_write_command(LCD_ENTRYMODESET | _showmode);  // Send the command to update the entry mode
}

// Enable auto-scrolling of text
LCD_STATUS autoscroll(void)
{
    _showmode |= LCD_ENTRYSHIFTINCREMENT;  // Set the LCD_ENTRYSHIFTINCREMENT bit to enable auto-scroll
    return lcd_i2c_write_command(LCD_ENTRYMODESET | _showmode);  // Send the command to update the entry mode
}

// Create a custom character on the LCD
LCD_STATUS createChar(uint8_t location, uint8_t charmap[])
{
    location &= 0x7;  // We only have 8 locations (0-7) for custom characters
    lcd_i2c_write_command(LCD_SETCGRAMADDR | (location << 3));  // Set the address for the custom character in CGRAM

    uint8_t idx = 0;
    uint8_t success = 0;
    for (; idx < 8; idx++)
    {
    	if(LCD_OK == lcd_i2c_write_data(charmap[idx])) success++;
    }

    if(idx == success) return LCD_OK;
    return LCD_ERROR_CREATE_CHAR;
}




LCD_STATUS lcd_init()
{
	HAL_Delay(100);  // give LCD time to power up fully
        _showfunction |= LCD_2LINE;

    _numlines = 2;  // Set the number of lines
    _currline = 0;      // Start with the first line

    HAL_Delay(50);  // Wait for at least 50ms after powering up (ensures the display is ready)

    // Send the function set command to initialize the display
    lcd_i2c_write_command(LCD_FUNCTIONSET | _showfunction);
    HAL_Delay(10);  // Wait more than 4.1ms (as per the datasheet)

    // Send the function set command again to ensure it’s applied
    lcd_i2c_write_command(LCD_FUNCTIONSET | _showfunction);
    HAL_Delay(10);

    // Send the function set command one more time
    lcd_i2c_write_command(LCD_FUNCTIONSET | _showfunction);

    // Turn on the display without cursor or blinking
    _showcontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    display();  // Apply the display control settings

    clear();  // Clear the display and set the cursor to the home position

    // Set the entry mode for text direction and cursor movement (default left to right, no auto-scroll)
    _showmode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    lcd_i2c_write_command(LCD_ENTRYMODESET | _showmode);  // Set the entry mode for the display
    return LCD_OK;
}




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

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  /* USER CODE BEGIN 2 */





  backlight_init();
  HAL_Delay(500);




  lcd_init();
  set_cursor(0, 0);
  send_string("Waveshare");
  set_cursor(0, 1);
  send_string("Hello Josh!");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	  HAL_Delay(500);

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
