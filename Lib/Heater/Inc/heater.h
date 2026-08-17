/**
 * @file     heater.h
 * @brief    Heater control driver interface
 * @details
 * This module provides an abstraction layer for controlling
 * a heater through a GPIO-controlled relay.
 * The driver is independent from application-level control logic.
 * It provides:
 *  - Heater driver initialization
 *  - Heater ON/OFF control
 *  - Heater state management
 *  - Heater state feedback
 *  - Relay polarity abstraction
 *  - Output safety initialization
 * Hardware configuration:
 *  - Heater              : Relay-controlled
 *  - Control GPIO        : HEATER_RELAY_GPIO_Port / HEATER_RELAY_Pin
 *  - Relay type          : Active-Low
 * Active-Low relay behavior:
 *      GPIO LOW   -> Relay ON  -> Heater ON
 *      GPIO HIGH  -> Relay OFF -> Heater OFF
 * GPIO configuration is assumed to be performed by STM32CubeMX.
 * @author   Fatemeh Moghadasian
 * @version  1.2
 */

#ifndef HEATER_H
#define HEATER_H


#include "main.h"
#include <stdbool.h>
#include <stdint.h>



/* Heater Configuration */


/**
 * @brief GPIO level that activates the heater relay.
 * The relay used in this project is Active-Low.
 * Therefore:
 *      GPIO_PIN_RESET -> Heater ON
 *      GPIO_PIN_SET   -> Heater OFF
 */
#define HEATER_ACTIVE_LEVEL       GPIO_PIN_RESET


/**
 * @brief GPIO level that deactivates the heater relay.
 * This is automatically derived from HEATER_ACTIVE_LEVEL.
 */
#define HEATER_INACTIVE_LEVEL     GPIO_PIN_SET



/* Heater State */


/**
 * @brief Heater logical operating state.
 * These values represent the software state of the heater.
 * They are intentionally independent from the electrical GPIO polarity.
 */
typedef enum
{
    HEATER_OFF = 0U,       /**< Heater is OFF */
    HEATER_ON  = 1U        /**< Heater is ON  */

} Heater_State_t;



/* Heater Status                                                              */


/**
 * @brief Heater driver status.
 * This status describes whether the heater driver is initialized
 * and operating normally.
 */
typedef enum
{
    HEATER_STATUS_NOT_INITIALIZED = 0U,
    HEATER_STATUS_OK,
    HEATER_STATUS_INVALID_STATE

} Heater_Status_t;



/* Heater Data */


/**
 * @brief Processed heater status information.
 * This structure provides the current logical state and
 * driver status of the heater.
 */
typedef struct
{
    Heater_State_t  state;       /**< Current heater logical state */
    Heater_Status_t status;      /**< Current driver status */

} Heater_Data_t;


/* Function Prototypes */


/**
 * @brief Initialize the heater driver.
 * The heater is forced to the OFF state during initialization
 * for safety.
 * @note
 * GPIO must already be initialized by STM32CubeMX before
 * calling this function.
 */
void Heater_Init(void);


/**
 * @brief Turn the heater ON.
 * For the Active-Low relay used in this project:
 *      GPIO = LOW
 *      Relay = ON
 *      Heater = ON
 */
void Heater_On(void);


/**
 * @brief Turn the heater OFF.
 * For the Active-Low relay used in this project:
 *      GPIO = HIGH
 *      Relay = OFF
 *      Heater = OFF
 */
void Heater_Off(void);


/**
 * @brief Set the heater logical state.
 * @param state Desired heater state.
 * @note
 * Invalid values are ignored.
 */
void Heater_SetState(Heater_State_t state);


/**
 * @brief Get the current heater logical state.
 * @return Current Heater_State_t value.
 */
Heater_State_t Heater_GetState(void);


/**
 * @brief Get complete heater driver data.
 * @return Heater_Data_t containing current state and status.
 */
Heater_Data_t Heater_GetData(void);


/**
 * @brief Get current heater driver status.
 * @return Current Heater_Status_t value.
 */
Heater_Status_t Heater_GetStatus(void);


/**
 * @brief Check whether heater driver is initialized.
 * @return
 *      true  -> driver initialized
 *      false -> driver not initialized
 */
bool Heater_IsInitialized(void);


/**
 * @brief Check whether heater is currently ON.
 * @return
 *      true  -> heater is ON
 *      false -> heater is OFF
 */
bool Heater_IsOn(void);


/**
 * @brief Check whether heater is currently OFF.
 * @return
 *      true  -> heater is OFF
 *      false -> heater is ON
 */
bool Heater_IsOff(void);


/**
 * @brief Force heater to safe OFF state.
 * This function is intended for safety handling.
 * It immediately disables the heater output and updates
 * the internal logical state.
 */
void Heater_SafeOff(void);


#endif /* HEATER_H */