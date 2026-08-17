/**
 * @file     lcd.h
 * @brief    Character LCD driver interface
 * @details
 * This module provides a abstraction layer for controlling a 
 * 16x2 character LCD based on the HD44780 controller.
 * The LCD is operated in 4-bit parallel interface mode.
 * Features:
 *  - LCD driver initialization
 *  - 4-bit parallel communication
 *  - LCD command transmission
 *  - Character transmission
 *  - Cursor positioning
 *  - Display clearing
 *  - Integer value display
 *  - Floating-point value display
 *  - LCD line clearing
 * The driver depends on the STM32 HAL GPIO interface
 * The GPIO configuration is assumed to be performed by
 * STM32CubeMX before the LCD driver is initialized.
 * Hardware configuration:
 *  -LCD                   : 1602 character LCD
 *  -Controller            : HD44780-compatible
 *  -Interface             : 4-bit parallel
 *  -Display Size          : 16 columns x 2 rows
 *  -RS Pin                : GPIO output
 *  -EN Pin                : GPIO output    
 *  -D4 Pin                : GPIO output
 *  -D5 Pin                : GPIO output
 *  -D6 Pin                : GPIO output
 *  -D7 Pin                : GPIO output
 *  -RW Pin                : Grounded (write-only mode)
 * @author   Fatemeh Moghadasian
 * @version  1.0
 */

 #ifndef LCD_H
 #define LCD_H

 #include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>


/* LCD  Pin Configuration */

#define LCD_RS_GPIO_Port   GPIOA
#define LCD_RS_Pin         GPIO_PIN_9

#define LCD_EN_GPIO_Port   GPIOC
#define LCD_EN_Pin         GPIO_PIN_7

#define LCD_D4_GPIO_Port   GPIOB
#define LCD_D4_Pin         GPIO_PIN_5

#define LCD_D5_GPIO_Port   GPIOB
#define LCD_D5_Pin         GPIO_PIN_4

#define LCD_D6_GPIO_Port   GPIOB
#define LCD_D6_Pin         GPIO_PIN_10

#define LCD_D7_GPIO_Port   GPIOA
#define LCD_D7_Pin         GPIO_PIN_8


/* LCD Dimensions */
#define LCD_COLUMNS  16U
#define LCD_ROWS 2U


/* Function Prototypes */


/**
 * @brief  Initialize the LCD dirver
 * This function initializes the HD44780-COMPATIBLE LCD in
 * 4-bit parallel communication mode.
 * The following LCD configuration is applied:
 * - 4-bit data interface
 * - 2 display lines
 * - 5x8 dot character font
 * - display enabled
 * - cursor disabled
 * - Cursor blinking disabled
 * @note  GPIO pins must already be initialized before calling this function.
 * @note  This function uses HAL_Delay() during initialization.
 */
void LCD_Init(void);


/**
 * @brief Clear the LCD display
 * Clears all characters from the LCD and return the cursor to
 * the home position
 * @note  The LCD clear command requires a longer execution time
 *        than normal LCD commands.
 */
void LCD_Clear(void);


/**
 * @brief Set LCD cursor position
 * @param row    LCD row : 0 or 1
 * @param column LCD column: 0 to 15
 * @note  Invalid row or column values are ignored
 */
void LCD_SetCursor(uint8_t row, uint8_t column);


/**
 * @brief Write one character to the LCD
 * @param character ASCII character to be displayed
 */
void LCD_WriteChar(char character);

/**
 * @brief Write a null-terminated string to the LCD
 * @param text Pointer to a null-terminated ASCII string
 * @note  If text is NULL, the function does nothing
 * @note  The LCD has a maximum visible width of 16 characters per line
 */
void LCD_Print(const char *text);


/**
 * @brief Display a signed integer value
 * @param value Integer value to be displayed
 */
void LCD_PrintInt(int32_t value);


/**
 * @brief Display a floating-point value with specified decimal places
 * @param value   Floating-point value
 * @param decimal Number of digits displayed after the decimal point
 * @example 
 *    LCD_PrintFloat(27.35f, 2U);
 *    Displays: 27.35
 */
void LCD_PrintFloat(float value, uint8_t decimal);


/**
 * @brief Clear the current LCD line
 * This function writes spaces over all 16 positions of the selected row
 * @param row LCD row : 0 or 1
 */
void LCD_ClearLine(uint8_t row);


#endif /* LCD_H */