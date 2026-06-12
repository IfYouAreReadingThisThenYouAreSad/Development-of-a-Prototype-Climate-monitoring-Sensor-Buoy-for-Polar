/*
 *  waveshare_lcd.c
 *
 *  Created on: 13 Mar 2026
 *      Author: thomasneedham
 */



#include "waveshare_lcd.h"





/* I2C Addresses */
#define LCD_ADDRESS               (0x3E << 1)    // I2C address of LCD
#define LCD_LIGHT_ADDRESS         (0x6B << 1)    // I2C address of LCD backlight

/* LCD Command Set */
#define LCD_CLEAR_DISPLAY         0x01           // Command to clear the display
#define LCD_RETURN_HOME            0x02           // Command to return cursor to home position
#define LCD_ENTRY_MODE_SET          0x04           // Command to set entry mode
#define LCD_DISPLAY_CONTROL        0x08           // Command to control display settings
#define LCD_CURSOR_SHIFT          0x10           // Command to shift cursor or display
#define LCD_FUNCTION_SET           0x20           // Command to set function (display mode)
#define LCD_SET_CCRAM_ADDR          0x40           // Command to set CGRAM address
#define LCD_SET_DDRAM_ADDR          0x80           // Command to set DDRAM address

/* Entry Mode Flags */
#define LCD_ENTRY_RIGHT            0x00           // Cursor moves right
#define LCD_ENTRY_LEFT             0x02           // Cursor moves left
#define LCD_ENTRY_SHIFT_INCREMENT   0x01           // Shift display to the right automatically
#define LCD_ENTRY_SHIFT_DECREMENT   0x00           // Shift display to the left automatically

/* Display Control Flags */
#define LCD_DISPLAY_ON             0x04           // Turn the display on
#define LCD_DISPLAY_OFF            0x00           // Turn the display off
#define LCD_CURSOR_ON              0x02           // Show the cursor
#define LCD_CURSOR_OFF             0x00           // Hide the cursor
#define LCD_BLINK_ON               0x01           // Blink the cursor
#define LCD_BLINK_OFF              0x00           // Do not blink the cursor

/* Cursor and Display Shift Flags */
#define LCD_DISPLAY_MOVE           0x08           // Move the display
#define LCD_CURSOR_MOVE            0x00           // Move the cursor
#define LCD_MOVE_RIGHT             0x04           // Move display/cursor to the right
#define LCD_MOVE_LEFT              0x00           // Move display/cursor to the left

/* Function Set Flags */
#define LCD_8BITMODE              0x10           // Set 8-bit mode
#define LCD_4BITMODE              0x00           // Set 4-bit mode
#define LCD_2LINE                 0x08           // Use 2 lines
#define LCD_1LINE                 0x00           // Use 1 line
#define LCD_5X8DOTS               0x00           // Set 5x8 dots for characters

/* SN3193 LED Controller I2C Address */
#define SN3193_IIC_ADDRESS        0x6B           // I2C address of SN3193 backlight controller

/* SN3193 Registers */
#define SHUTDOWN_REG              0x00           // Software shutdown register
#define BREATHING_CONTROL_REG     0x01           // Breathing effect control register
#define LED_MODE_REG              0x02           // LED mode control register
#define LED_NORMAL_MODE           0x00           // Normal LED operation
#define LED_BREATH_MODE           0x20           // Breathing LED operation
#define CURRENT_SETTING_REG       0x03           // Set output current
#define PWM_1_REG                 0x04           // PWM duty cycle for channel 1
#define PWM_2_REG                 0x05           // PWM duty cycle for channel 2
#define PWM_3_REG                 0x06           // PWM duty cycle for channel 3
#define PWM_UPDATE_REG            0x07           // Load PWM settings

#define T0_1_REG                  0x0A           // Set T0 time for OUT1
#define T0_2_REG                  0x0B           // Set T0 time for OUT2
#define T0_3_REG                  0x0C           // Set T0 time for OUT3

#define T1T2_1_REG                0x10           // Set T1&T2 time for OUT1
#define T1T2_2_REG                0x11           // Set T1&T2 time for OUT2
#define T1T2_3_REG                0x12           // Set T1&T2 time for OUT3

#define T3T4_1_REG                0x16           // Set T3&T4 time for OUT1
#define T3T4_2_REG                0x17           // Set T3&T4 time for OUT2
#define T3T4_3_REG                0x18           // Set T3&T4 time for OUT3

#define TIME_UPDATE_REG           0x1C           // Load time registers
#define LED_CONTROL_REG           0x1D           // Enable LED outputs OUT1~OUT3
#define RESET_REG                 0x2F           // Reset all registers to default values






WaveShareLCD::WaveShareLCD(I2C_HandleTypeDef *i2c_pin_assigment){
	lcd_i2c_assigment = i2c_pin_assigment;
    _showfunction = 0;
    _showcontrol  = 0;
    _showmode     = 0;
    _numlines     = 0;
    _currline     = 0;

	_initStatus = backlight_init();

	if(_initStatus != LCD_OK) return;


	_initStatus = lcd_init();

	if(_initStatus != LCD_OK) return;


}






//sends a command to the screen
LCD_STATUS WaveShareLCD::lcd_i2c_write_command(uint8_t function_byte) { // used for sending commands
    uint8_t sending_buffer[2];
    sending_buffer[0] = 0x80;
    sending_buffer[1] = function_byte;

    HAL_StatusTypeDef i2c_status =
        HAL_I2C_Master_Transmit(lcd_i2c_assigment, LCD_ADDRESS, sending_buffer, 2, HAL_MAX_DELAY);

    HAL_Delay(5);  // small delay after every command

    if (i2c_status == HAL_OK) return LCD_OK;
    return LCD_ERROR_I2C;
}



//sends data to the screen
LCD_STATUS WaveShareLCD::lcd_i2c_write_data(uint8_t function_byte) { // used for sending character
    uint8_t sending_buffer[2];
    sending_buffer[0] = 0x40;
    sending_buffer[1] = function_byte;

    HAL_StatusTypeDef i2c_status =
        HAL_I2C_Master_Transmit(lcd_i2c_assigment, LCD_ADDRESS, sending_buffer, 2, HAL_MAX_DELAY);

    if (i2c_status == HAL_OK) return LCD_OK;
    return LCD_ERROR_I2C;
}



// sends a command to the backlight chip
LCD_STATUS WaveShareLCD::i2c_write_backlight(uint8_t reg, uint8_t val) {
    uint8_t buffer[2];
    buffer[0] = reg;
    buffer[1] = val;

    HAL_StatusTypeDef i2c_status =
        HAL_I2C_Master_Transmit(lcd_i2c_assigment, LCD_LIGHT_ADDRESS, buffer, 2, HAL_MAX_DELAY);

    if (HAL_OK == i2c_status) return LCD_OK;
    return LCD_ERROR_I2C_REG;
}




//inilizates the black light of the screen
LCD_STATUS WaveShareLCD::backlight_init(void) {
    i2c_write_backlight(RESET_REG, 0x00);
    HAL_Delay(10);

    if (LCD_OK != i2c_write_backlight(SHUTDOWN_REG, 0x20)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(LED_MODE_REG, LED_NORMAL_MODE)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(CURRENT_SETTING_REG, 0x00)) return LCD_ERROR_I2C;
    HAL_Delay(10);

    if (LCD_OK != i2c_write_backlight(PWM_1_REG, 0xFF)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    if (LCD_OK != i2c_write_backlight(PWM_UPDATE_REG, 0x00)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(T0_1_REG, 0x40)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(T0_2_REG, 0x40)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(T0_3_REG, 0x40)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    if (LCD_OK != i2c_write_backlight(T1T2_1_REG, 0x26)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(T1T2_2_REG, 0x26)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(T1T2_3_REG, 0x26)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    if (LCD_OK != i2c_write_backlight(T3T4_1_REG, 0x26)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(T3T4_2_REG, 0x26)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(T3T4_3_REG, 0x26)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    if (LCD_OK != i2c_write_backlight(LED_CONTROL_REG, 0x01)) return LCD_ERROR_I2C;
    if (LCD_OK != i2c_write_backlight(TIME_UPDATE_REG, 0x00)) return LCD_ERROR_I2C;
    HAL_Delay(100);

    return LCD_OK;
}




LCD_STATUS WaveShareLCD::set_brightness(uint8_t value) {
    if (value > 100) {
        return LCD_ERROR_SETTING_BRIGHTNESS;
    }

    i2c_write_backlight(PWM_1_REG, (uint8_t)((float)value * 2.55));
    HAL_Delay(100);
    i2c_write_backlight(PWM_UPDATE_REG, 0x00);

    return LCD_OK;
}






LCD_STATUS WaveShareLCD::set_led_mode(uint8_t mode) {
    if (mode != 0x20 && mode != 0x00) return LCD_ERROR_SETTING_BACKLIGHT_MODE;
    return i2c_write_backlight(LED_MODE_REG, mode);
}

LCD_STATUS WaveShareLCD::display_on(void) {
    _showcontrol |= LCD_DISPLAY_ON;
    return lcd_i2c_write_command(LCD_DISPLAY_CONTROL | _showcontrol);
}

LCD_STATUS WaveShareLCD::clear(void) {
    LCD_STATUS command_status = lcd_i2c_write_command(LCD_CLEAR_DISPLAY);
    HAL_Delay(2);
    return command_status;
}

LCD_STATUS WaveShareLCD::set_cursor(uint8_t col, uint8_t row) {
    col = (row == 0 ? col | 0x80 : col | 0xC0);
    return lcd_i2c_write_command(col);
}

LCD_STATUS WaveShareLCD::data(uint8_t value) {
    return lcd_i2c_write_data(value);
}

LCD_STATUS WaveShareLCD::send_string(const char *str) {
    uint8_t i = 0, success = 0;
    for (; str[i] != '\0'; i++) {
        if (LCD_OK == lcd_i2c_write_data(str[i])) success++;
    }
    return (i == success) ? LCD_OK : LCD_ERROR_SENDING_STRING;
}

LCD_STATUS WaveShareLCD::no_cursor(void) {
    _showcontrol &= ~LCD_CURSOR_ON;
    return lcd_i2c_write_command(LCD_DISPLAY_CONTROL | _showcontrol);
}

LCD_STATUS WaveShareLCD::cursor(void) {
    _showcontrol |= LCD_CURSOR_ON;
    return lcd_i2c_write_command(LCD_DISPLAY_CONTROL | _showcontrol);
}

LCD_STATUS WaveShareLCD::scroll_display_left(void) {
    return lcd_i2c_write_command(LCD_CURSOR_SHIFT| LCD_DISPLAY_MOVE | LCD_MOVE_LEFT);
}

LCD_STATUS WaveShareLCD::scroll_display_right(void) {
    return lcd_i2c_write_command(LCD_CURSOR_SHIFT| LCD_DISPLAY_MOVE | LCD_MOVE_RIGHT);
}

LCD_STATUS WaveShareLCD::left_to_right(void) {
    _showmode |= LCD_ENTRY_LEFT;
    return lcd_i2c_write_command(LCD_ENTRY_MODE_SET | _showmode);
}

LCD_STATUS WaveShareLCD::right_to_left(void) {
    _showmode &= ~LCD_ENTRY_LEFT;
    return lcd_i2c_write_command(LCD_ENTRY_MODE_SET | _showmode);
}

LCD_STATUS WaveShareLCD::no_autoscroll(void) {
    _showmode &= ~LCD_ENTRY_SHIFT_INCREMENT;
    return lcd_i2c_write_command(LCD_ENTRY_MODE_SET | _showmode);
}

LCD_STATUS WaveShareLCD::autoscroll(void) {
    _showmode |= LCD_ENTRY_SHIFT_INCREMENT;
    return lcd_i2c_write_command(LCD_ENTRY_MODE_SET | _showmode);
}

LCD_STATUS WaveShareLCD::create_char(uint8_t location, uint8_t charmap[]) {
    location &= 0x07;
    lcd_i2c_write_command(LCD_SET_CCRAM_ADDR | (location << 3));

    uint8_t idx = 0, success = 0;
    for (; idx < 8; idx++) {
        if (LCD_OK == lcd_i2c_write_data(charmap[idx])) success++;
    }
    return (idx == success) ? LCD_OK : LCD_ERROR_CREATE_CHAR;
}


//sets up parameteres of the LCD screen
LCD_STATUS WaveShareLCD::lcd_init(void) {
    HAL_Delay(100);
    _showfunction |= LCD_2LINE;

    _numlines = 2;
    _currline = 0;

    HAL_Delay(50);
    lcd_i2c_write_command(LCD_FUNCTION_SET | _showfunction);
    HAL_Delay(10);
    lcd_i2c_write_command(LCD_FUNCTION_SET | _showfunction);
    HAL_Delay(10);
    lcd_i2c_write_command(LCD_FUNCTION_SET | _showfunction);

    _showcontrol = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
    display_on();
    clear();

    _showmode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DECREMENT;
    lcd_i2c_write_command(LCD_ENTRY_MODE_SET | _showmode);

    return LCD_OK;
}




LCD_STATUS WaveShareLCD::display_off(void){
    _showcontrol &= ~LCD_DISPLAY_ON;
    return lcd_i2c_write_command(LCD_DISPLAY_CONTROL | _showcontrol);
}





LCD_STATUS WaveShareLCD::display_logo(void){

    // C0: left outer slope — diagonal fills bottom half
    uint8_t c0[8] = {
        0b00000,
        0b00000,
        0b00000,
        0b00001,
        0b00011,
        0b00111,
        0b01111,
        0b11111
    };

    // C1: left inner slope continues diagonal into sub-peak
    uint8_t c1[8] = {
        0b00001,
        0b00011,
        0b00111,
        0b01111,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    // C2: main peak left side — diagonal into solid
    uint8_t c2[8] = {
        0b00100,
        0b01100,
        0b11000,
        0b10000,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    // C3: main peak centre — snow cap
    uint8_t c3[8] = {
        0b00100,
        0b01110,
        0b10011,
        0b00111,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    // C4: main peak right side (mirror C2)
    uint8_t c4[8] = {
        0b00100,
        0b00110,
        0b00011,
        0b00001,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    // C5: right inner slope (mirror C1)
    uint8_t c5[8] = {
        0b10000,
        0b11000,
        0b11100,
        0b11110,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    // C6: right outer slope (mirror C0)
    uint8_t c6[8] = {
        0b00000,
        0b00000,
        0b00000,
        0b10000,
        0b11000,
        0b11100,
        0b11110,
        0b11111
    };

    // C7: bottom left corner — diagonal continues from C0 row 0
    uint8_t c7[8] = {
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    if(LCD_OK != create_char(0, c0)) return LCD_ERROR_CREATE_CHAR;
    if(LCD_OK != create_char(1, c1)) return LCD_ERROR_CREATE_CHAR;
    if(LCD_OK != create_char(2, c2)) return LCD_ERROR_CREATE_CHAR;
    if(LCD_OK != create_char(3, c3)) return LCD_ERROR_CREATE_CHAR;
    if(LCD_OK != create_char(4, c4)) return LCD_ERROR_CREATE_CHAR;
    if(LCD_OK != create_char(5, c5)) return LCD_ERROR_CREATE_CHAR;
    if(LCD_OK != create_char(6, c6)) return LCD_ERROR_CREATE_CHAR;
    if(LCD_OK != create_char(7, c7)) return LCD_ERROR_CREATE_CHAR;

    // Row 0 — upper mountain, diagonal slopes no solid blocks
    // sp sp C0 C1 C2 C3 C3 C4 C5 C6 sp sp sp sp sp sp
    if(LCD_OK != set_cursor(2, 0)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0))    return LCD_ERROR_I2C;  // C0 outer left slope
    if(LCD_OK != data(1))    return LCD_ERROR_I2C;  // C1 inner left slope
    if(LCD_OK != data(2))    return LCD_ERROR_I2C;  // C2 peak left
    if(LCD_OK != data(3))    return LCD_ERROR_I2C;  // C3 peak centre
    if(LCD_OK != data(3))    return LCD_ERROR_I2C;  // C3 reused
    if(LCD_OK != data(4))    return LCD_ERROR_I2C;  // C4 peak right
    if(LCD_OK != data(5))    return LCD_ERROR_I2C;  // C5 inner right slope
    if(LCD_OK != data(6))    return LCD_ERROR_I2C;  // C6 outer right slope

    // Row 1 — solid base, 0xFF the full width
    if(LCD_OK != set_cursor(0, 1)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;
    if(LCD_OK != data(0xFF)) return LCD_ERROR_I2C;

    return LCD_OK;
}










