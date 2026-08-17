/**
 * @file     heater.c
 * @brief    Heater control driver implementation
 * @details
 * This module implements the GPIO-based heater driver.
 * The heater is controlled through an Active-Low relay:
 *      GPIO LOW  -> Relay ON  -> Heater ON
 *      GPIO HIGH -> Relay OFF -> Heater OFF
 * The driver contains no application-level temperature control.
 * Decisions such as:
 *      "Turn heater ON when temperature < 20 C"
 * belong to the FSM/control layer.
 * This module is responsible only for:
 *      Application command
 *              |
 *              v
 *        Heater Driver
 *              |
 *              v
 *          GPIO / Relay
 * @author   Fatemeh Moghadasian
 * @version  1.3
 */

#include "heater.h"



/* Private Variables */


/**
 * @brief Current logical heater state.
 */
static Heater_State_t heater_state = HEATER_OFF;


/**
 * @brief Current heater driver status.
 */
static Heater_Status_t heater_status =
    HEATER_STATUS_NOT_INITIALIZED;



/* Private Functions */


/**
 * @brief Write the physical relay output.
 * @param level GPIO output level.
 * @note
 * This function is kept private so that the rest of the application
 * does not need to know whether the relay is Active-Low or Active-High.
 */
static void Heater_WriteOutput(GPIO_PinState level)
{
    HAL_GPIO_WritePin(HEATER_RELAY_GPIO_Port,HEATER_RELAY_Pin,level);
}


/**
 * @brief Apply a logical heater state to the physical GPIO.
 * @param state Desired logical heater state.
 */
static void Heater_ApplyState(Heater_State_t state)
{
    if (state == HEATER_ON)
    {
        /*
         * Active-Low relay:
         * LOW -> Relay ON -> Heater ON
         */
        Heater_WriteOutput(HEATER_ACTIVE_LEVEL);
    }
    else
    {
        /*
         * HIGH -> Relay OFF -> Heater OFF
         */
        Heater_WriteOutput(HEATER_INACTIVE_LEVEL);
    }
}



/* Public Functions                                                           */


/**
 * @brief Initialize the heater driver.
 * The heater is always forced into the OFF state.
 */
void Heater_Init(void)
{
    /*
     * Safety requirement:
     * Heater must start OFF.
     */
    heater_state = HEATER_OFF;

    Heater_WriteOutput(HEATER_INACTIVE_LEVEL);

    heater_status = HEATER_STATUS_OK;
}


/**
 * @brief Turn heater ON.
 */
void Heater_On(void)
{
    /*
     * Do not operate the driver before initialization.
     */
    if (heater_status != HEATER_STATUS_OK)
    {
        return;
    }

    /*
     * Update logical state.
     */
    heater_state = HEATER_ON;

    /*
     * Apply physical relay state.
     */
    Heater_ApplyState(HEATER_ON);
}


/**
 * @brief Turn heater OFF.
 */
void Heater_Off(void)
{
    /*
     * Even if the driver is not initialized,
     * the output should be forced OFF.
     */
    heater_state = HEATER_OFF;

    Heater_WriteOutput(HEATER_INACTIVE_LEVEL);

    /*
     * Only report normal status if initialization
     * has already occurred.
     */
    if (heater_status == HEATER_STATUS_OK)
    {
        heater_status = HEATER_STATUS_OK;
    }
}


/**
 * @brief Set heater logical state.
 *
 * @param state Desired heater state.
 */
void Heater_SetState(Heater_State_t state)
{
    switch (state)
    {
        case HEATER_ON:
            Heater_On();
            break;

        case HEATER_OFF:
            Heater_Off();
            break;


        default:
            /*
             * Invalid state.
             *
             * For safety, turn heater OFF.
             */
            Heater_SafeOff();
            heater_status = HEATER_STATUS_INVALID_STATE;
            break;
    }
}


/**
 * @brief Get current logical heater state.
 */
Heater_State_t Heater_GetState(void)
{
    return heater_state;
}


/**
 * @brief Get complete heater data.
 */
Heater_Data_t Heater_GetData(void)
{
    Heater_Data_t data;

    data.state  = heater_state;
    data.status = heater_status;

    return data;
}


/**
 * @brief Get current heater driver status.
 */
Heater_Status_t Heater_GetStatus(void)
{
    return heater_status;
}


/**
 * @brief Check whether driver is initialized.
 */
bool Heater_IsInitialized(void)
{
    return (heater_status == HEATER_STATUS_OK);
}


/**
 * @brief Check whether heater is ON.
 */
bool Heater_IsOn(void)
{
    return (heater_state == HEATER_ON);
}


/**
 * @brief Check whether heater is OFF.
 */
bool Heater_IsOff(void)
{
    return (heater_state == HEATER_OFF);
}


/**
 * @brief Force heater into safe OFF state.
 */
void Heater_SafeOff(void)
{
    /*
     * Physical safety first:
     * force relay inactive.
     */
    Heater_WriteOutput(HEATER_INACTIVE_LEVEL);

    /*
     * Update logical state.
     */
    heater_state = HEATER_OFF;
}