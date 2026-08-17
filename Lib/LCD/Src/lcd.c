/**
 * @file     lcd.c
 * @brief    Character LCD 1602 driver implementation
 * @details
 * This module implements the driver for a 16x2 character LCD
 * using an HD44780-compatible controller
 * The LCD is controlled through a 4-bit parallel interface
 * The driver is responsible only for LCD communication and
 * display control. It does not contain application-specific
 * sensor or control logic
 * Hardware communivastion:
 *    STM32 GPIO
 *         |
 *         + --- RS
 *         + --- EN
 *         + --- D4
 *         + --- D5
 *         + --- D6
 *         + --- D7
 *         |
 *         V
 *     LCD 1602
 * RW is assumed to be connected to GND because this driver
 * performs write-only communication with the LCD
 * @author   Fatemeh Moghadasian
 * @version  1.0
 */

#include "lcd.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>

/* Private Function Prototypes */

/**
 * @brief Send a 4-bit data nibble to the LCD
 * @param data Four-bit value
 * @note  Only the lower four bits of data are used
 */
static void LCD_Send4Bits(uint8_t data);


/**
 * @brief Generate LCD enable pulse
 * The LCD captures the data placed on D4-D7 when the Enable
 * signal transitions from HIGH to LOW
 */
static void LCD_EnablePulse(void);


/**
 * @brief Send an instruction command to the LCD
 * @param command HD44780 LCD command byte
 */
static void LCD_SendCommand(uint8_t command);


/**
 * @brief Send display data to the LCD
 * @param data Character data byte
 */
static void LCD_SendData(uint8_t data);


/* Public Functions */

/**
 * @brief  Initialize the LCD dirver
 */
void LCD_Init(void)
{
    /*
     * Step 1: Wait for LCD power-up stabilization
     */
    HAL_Delay(40U);

    /*
     * Step 2: Initialize control lines
     */
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);

    /*
     * Step 3: HD44780 initialization sequence.
     * At power-up the LCD may initially be in 8-bit mode
     * The sequence below forves the controller into 4-bit mode
     */
    LCD_Send4Bits(0x03U);
    HAL_Delay(5U);
    LCD_Send4Bits(0x03U);
    HAL_Delay(1U);
    LCD_Send4Bits(0x03U);
    HAL_Delay(1U);

    /*
     * select 4-bit comunication mode
     */
    LCD_Send4Bits(0x02U);
    HAL_Delay(1U);


    /*
     * Step 4: Function set
     * 0x28:
     *    DL = 0 -> 4bit interface
     *    N  = 1 -> 2- line display
     *    F  = 0 -> 5x8 character font
     */
    LCD_SendCommand(0x28U);

    /*
     * Step 5: Display off
     */
    LCD_SendCommand(0x08U);

    /*
     * Step 6: Clear display
     */
    LCD_SendCommand(0x01U);
    HAL_Delay(2U);

    /*
     * Step 7: Entry mode set
     * Cursor moves from left to right
     * Display is not shifted
     */
    LCD_SendCommand(0x06U);

    /*
     * Step 8: Display on
     * Display  = ON
     * Cursor   = OFF
     * Blink    = OFF
     */
    LCD_SendCommand(0x0CU);
}


/**
 * @brief Clear the LCD display
 */
void LCD_Clear(void)
{
    /* HD44780 clear-display command */
    LCD_SendCommand(0x01U);

    /* The clear command requries approxomately 1.5 ms*/
    HAL_Delay(2U); 
}


/**
 * @brief Set LCD cursor position
 */
void LCD_SetCursor(uint8_t row, uint8_t column)
{
    uint8_t address;

    if (row >= LCD_ROWS)
    {
        return;
    }

    if (column >= LCD_COLUMNS)
    {
        return;
    }

    if (row == 0U)
    {
        address = column;
    }
    else
    {
        address = 0x40U + column;
    }

    LCD_SendCommand(0x80U | address);
}


/**
 * @brief Write one character to the LCD
 */
void LCD_WriteChar(char character)
{
    LCD_SendData((uint8_t) character);
}


/**
 * @brief Write a null-terminated string to the LCD
 */
void LCD_Print(const char *text)
{
    /*
     * Protect against NULL pointer
     */
    if (text == NULL)
    {
        return;
    }

    /*
     * Send characters until the null terminator
     */
    while (*text != '\0') 
    {
        LCD_WriteChar(*text);
        text++;
    }

}


/**
 * @brief Display a signed integer value
 */
void LCD_PrintInt(int32_t value)
{
    char buffer[16];

    /*
     * Convert integer to ASCII representation 
     */
    snprintf(buffer, sizeof(buffer), "%ld", (long)value);

    /*
     * Display converted string
     */
    LCD_Print(buffer);

}


/**
 * @brief Display a floating-point value with specified decimal places
 */
void LCD_PrintFloat(float value, uint8_t decimal)
{
    char buffer[24];

    /*
     * Convert floating-point value into a formatted string
     * Example:
     * value = 27.35
     * decimals = 2
     * Result:
     * "27.35"
     */
    snprintf(buffer, sizeof(buffer), "%.*f", decimal, (double)value);

    /*
     * Display formatted value
     */
    LCD_Print(buffer);
}


/**
 * @brief Clear the current LCD line
 */
void LCD_ClearLine(uint8_t row)
{
    uint8_t i;

    // validate row
    if(row >= LCD_ROWS)
    {
        return;
    }

    /*
     * Move cursor to beginning of selected row
     */
    LCD_SetCursor(row, 0U);

    /*
     * Overwrite all 16 character positions with spaces
     */
    for (i = 0U; i<LCD_COLUMNS; i++)
    {
        LCD_WriteChar(' ');
    }

    /*
     * Return cursor to beginning of the row
     */
    LCD_SetCursor(row, 0U);

}




/* Private Functions */


/**
 * @brief Send a 4-bit data nibble to the LCD
 */
static void LCD_Send4Bits(uint8_t data)
{
    /*
     * D4 = bit 0
     */
    HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin,
                      (data & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    
    /*
     * D5 = bit 1
     */
    HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin,
                      (data & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    
    /*
     * D6 = bit 2
     */
    HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin,
                      (data & 0x04U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /*
     * D7 = bit 3
     */
    HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin,
                      (data & 0x08U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /*
     * Transfer the nibble to the LCD
     */
    LCD_EnablePulse();
}



/**
 * @brief Generate LCD enable pulse
 */
static void LCD_EnablePulse(void)
{
    /*
     * EN HIGH
     */
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);

    /*
     * Short enable pulse
     */
    HAL_Delay(1U);

    /*
     * EN LOW
     * LCD captures the data
     */
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);

    /*
     * Allow LCD internal operation ti settle
     */
    HAL_Delay(1U);

}


/**
 * @brief Send an instruction command to the LCD
 */
static void LCD_SendCommand(uint8_t command)
{
    /*
     * RS = 0
     * Indicates command mode
     */
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);

    /*
     * Send high nibble
     */
    LCD_Send4Bits(command >> 4U);

    /*
     * Send low nibble
     */
    LCD_Send4Bits(command & 0x0FU);

}


/**
 * @brief Send display data to the LCD
 */
static void LCD_SendData(uint8_t data)
{
    /*
     * RS = 1
     * Indicates character/data mode
     */
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET);

    /*
     * Send high nibble
     */
    LCD_Send4Bits(data >> 4U);

    /*
     * Send low nibble
     */
    LCD_Send4Bits(data & 0x0FU);



}
