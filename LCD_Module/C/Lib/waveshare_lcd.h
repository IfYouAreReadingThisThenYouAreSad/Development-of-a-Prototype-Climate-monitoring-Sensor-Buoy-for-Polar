/*
 * waveshare_lcd.h
 *
 *  Created on: 13 Mar 2026
 *      Author: thomasneedham
 */

#ifndef INC_WAVESHARE_LCD_H_
#define INC_WAVESHARE_LCD_H_


#ifdef __cpluscplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"




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
	LCD_ERROR_CREATE_CHAR,
	LCD_ERROR_BACKLIGHT_INIT,
	LCD_ERROR_LCD_INIT
}LCD_STATUS;






/**
 * @brief Tunes the brightness of the screen.
 *
 * @param value An unsigned byte integer from 0 to 100
 *
 * @return LCD_OK if successful or LCD_ERROR_SETTING_BRIGHTNESS if passed a value higher than 100
 */
LCD_STATUS set_brightness(uint8_t value);

/**
 * @brief Sets the LED backlight mode.
 *
 * @param mode 0x00 for normal steady mode, 0x20 for breathing mode
 *
 * @return LCD_OK if successful or LCD_ERROR_SETTING_BACKLIGHT_MODE if invalid mode passed
 */
LCD_STATUS set_led_mode(uint8_t mode);

/**
 * @brief Turns the display on. Restores previously displayed content.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS display_on(void);

/**
 * @brief Turns the display off. Content is preserved in memory and restored on display_on().
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS display_off(void);

/**
 * @brief Clears all characters from the display and resets cursor to row 0, col 0.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS clear(void);

/**
 * @brief Moves the cursor to a specified position on the display.
 *
 * @param col Column position (0-15)
 * @param row Row position (0 = top row, 1 = bottom row)
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS set_cursor(uint8_t col, uint8_t row);

/**
 * @brief Sends a single character to the display at the current cursor position.
 *
 * @param value ASCII value of the character to display
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS data(uint8_t value);

/**
 * @brief Sends a null-terminated string to the display at the current cursor position.
 *
 * @param str Pointer to the string to display. Max 16 characters per line.
 *
 * @return LCD_OK if all characters sent successfully or LCD_ERROR_SENDING_STRING if any character failed
 */
LCD_STATUS send_string(const char *str);

/**
 * @brief Hides the cursor from the display.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS no_cursor(void);

/**
 * @brief Shows the cursor on the display.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS cursor(void);

/**
 * @brief Scrolls the entire display content one position to the left.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS scroll_display_left(void);

/**
 * @brief Scrolls the entire display content one position to the right.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS scroll_display_right(void);

/**
 * @brief Sets text direction to left-to-right. Cursor moves right after each character.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS left_to_right(void);

/**
 * @brief Sets text direction to right-to-left. Cursor moves left after each character.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS right_to_left(void);

/**
 * @brief Disables auto-scrolling. Display does not shift when new characters are written.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS no_autoscroll(void);

/**
 * @brief Enables auto-scrolling. Display shifts to make room for each new character written.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if transmission failed
 */
LCD_STATUS autoscroll(void);

/**
 * @brief Creates a custom character and stores it in CGRAM.
 *
 * @param location CGRAM slot to store the character (0-7)
 * @param charmap  Array of 8 bytes defining the 5x8 pixel pattern of the character
 *
 * @return LCD_OK if successful or LCD_ERROR_CREATE_CHAR if any byte failed to send
 */
LCD_STATUS create_char(uint8_t location, uint8_t charmap[]);



/**
 * @brief Initialises the backlight and LCD display.
 *        Must be called before any other LCD functions.
 *
 * Note Configures 2-line mode, 5x8 font, display on, cursor off, left-to-right entry mode.
 *
 * @return LCD_OK if successful or LCD_ERROR_I2C if either init failed
 */
LCD_STATUS lcd_startup(I2C_HandleTypeDef *i2c_pin_assigment);






LCD_STATUS display_logo(void);






























#ifdef __cplusplus
}
#endif

#endif /* INC_WAVESHARE_LCD_H_ */
