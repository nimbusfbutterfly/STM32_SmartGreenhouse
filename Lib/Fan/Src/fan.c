/**
 * @file     fan.c
 * @brief    DC fan PWM driver implementation
 * @details
 * This module implements PWM-based speed control for a DC fan.
 * The driver converts a requested speed percentage into
 * a timer compare value using the configured timer ARR.
 * Mathematical relationship:
 *     Compare = Speed(%) * ARR / 100
 * Example for ARR = 4499:
 *     0%   -> 0
 *     25%  -> 1124
 *     50%  -> 2249
 *     75%  -> 3374
 *     100% -> 4499
 * The driver only controls the PWM command.
 * It does not measure actual motor speed.
 * Therefore FAN_ON means that a non-zero PWM command is being
 * generated, not that the motor has been physically verified
 * to be rotating.
 * @author   Fatemeh Moghadasian
 * @version  1.7
 */

#include "fan.h"



/* Private Variables */


/**
 * @brief Timer handle supplied by CubeMX.
 */
static TIM_HandleTypeDef *fan_htim = NULL;


/**
 * @brief PWM channel used by the fan.
 */
static uint32_t fan_channel = 0U;


/**
 * @brief Complete fan driver data.
 */
static Fan_Data_t fan_data =
{
    .state         = FAN_OFF,
    .status        = FAN_STATUS_NOT_INITIALIZED,
    .speed_percent = FAN_DEFAULT_SPEED_PERCENT,
    .compare_value = 0U,
    .auto_reload   = 0U
};



/* Private Functions */

/**
 * @brief Validate timer handle and PWM channel.
 * @return
 *        true  -> configuration is valid
 *        false -> configuration is invalid
 */
static bool Fan_IsConfigurationValid(void)
{
    if (fan_htim == NULL)
    {
        return false;
    }

    if (fan_htim->Instance == NULL)
    {
        return false;
    }

    if ((fan_channel != TIM_CHANNEL_1) &&
        (fan_channel != TIM_CHANNEL_2) &&
        (fan_channel != TIM_CHANNEL_3) &&
        (fan_channel != TIM_CHANNEL_4))
    {
        return false;
    }

    return true;
}



/**
 * @brief Convert speed percentage to PWM compare value.
 * @param speed_percent Fan speed from 0 to 100%.
 * @return
 *        Corresponding timer compare value.
 * @details
 * The timer ARR defines the maximum PWM count.
 * For example:
 * ARR = 4499
 * 50%:
 *     compare = 50 * 4499 / 100
 *             = 2249
 */
static uint32_t Fan_SpeedToCompare(uint8_t speed_percent)
{
    uint32_t arr;
    uint32_t compare;

    if (fan_htim == NULL)
    {
        return 0U;
    }

    /*
     * Read current timer auto-reload value.
     */
    arr = __HAL_TIM_GET_AUTORELOAD(fan_htim);

    /*
     * Convert percentage to timer compare value.
     */
    compare =
        ((uint32_t)speed_percent * arr) / 100U;

    /*
     * Protect against an out-of-range compare value.
     */
    if (compare > arr)
    {
        compare = arr;
    }

    return compare;
}


/**
 * @brief Apply a PWM compare value.
 * @param compare PWM compare value.
 */
static void Fan_WriteCompare(uint32_t compare)
{
    __HAL_TIM_SET_COMPARE(fan_htim, fan_channel, compare);
}


/**
 * @brief Update internal PWM information.
 * @param compare Current compare value.
 */
static void Fan_UpdateCompare(uint32_t compare)
{
    fan_data.compare_value = compare;

    if (fan_htim != NULL)
    {
        fan_data.auto_reload =
            __HAL_TIM_GET_AUTORELOAD(fan_htim);
    }
}



/* Public Functions */


/**
 * @brief Initialize fan driver.
 */
HAL_StatusTypeDef Fan_Init(TIM_HandleTypeDef *htim, uint32_t channel)
{
    /*
     * Check timer handle.
     */
    if (htim == NULL)
    {
        return HAL_ERROR;
    }

    /*
     * Check timer instance.
     */
    if (htim->Instance == NULL)
    {
        return HAL_ERROR;
    }

    /*
     * IMPORTANT:
     *
     * TIM_CHANNEL_1 is defined by STM32 HAL as 0x00000000U.
     *
     * Therefore:
     *
     *     channel == 0
     *
     * DOES NOT mean invalid channel.
     *
     * TIM_CHANNEL_1 is a valid channel.
     */

    /*
     * Accept only valid STM32 timer channels.
     */
    if ((channel != TIM_CHANNEL_1) &&
        (channel != TIM_CHANNEL_2) &&
        (channel != TIM_CHANNEL_3) &&
        (channel != TIM_CHANNEL_4))
    {
        return HAL_ERROR;
    }

    /*
     * Store timer configuration.
     */
    fan_htim = htim;
    fan_channel = channel;

    /*
     * Read timer Auto Reload Register.
     */
    fan_data.auto_reload =
        __HAL_TIM_GET_AUTORELOAD(fan_htim);

    /*
     * ARR must be non-zero for PWM operation.
     */
    if (fan_data.auto_reload == 0U)
    {
        fan_htim = NULL;
        fan_channel = 0U;

        fan_data.state = FAN_OFF;
        fan_data.status = FAN_STATUS_NOT_INITIALIZED;
        fan_data.speed_percent = FAN_DEFAULT_SPEED_PERCENT;
        fan_data.compare_value = 0U;
        fan_data.auto_reload = 0U;

        return HAL_ERROR;
    }

    /*
     * Initialize software state.
     */
    fan_data.state = FAN_OFF;

    fan_data.status = FAN_STATUS_OFF;

    fan_data.speed_percent =
        FAN_DEFAULT_SPEED_PERCENT;

    fan_data.compare_value = 0U;

    /*
     * Force PWM compare value to zero.
     *
     * This guarantees that the fan starts in a safe
     * OFF condition.
     */
    __HAL_TIM_SET_COMPARE(
        fan_htim,
        fan_channel,
        0U
    );

    return HAL_OK;
}


/**
 * @brief Start PWM generation.
 */
HAL_StatusTypeDef Fan_Start(void)
{
    HAL_StatusTypeDef hal_status;

    /*
     * Check initialization.
     */
    if (!Fan_IsConfigurationValid())
    {
        fan_data.status = FAN_STATUS_NOT_INITIALIZED;
        return HAL_ERROR;
    }

    /*
     * Start PWM output.
     */
    hal_status = HAL_TIM_PWM_Start(fan_htim,fan_channel);

    /*
     * Check HAL result.
     */
    if (hal_status != HAL_OK)
    {
        fan_data.status = FAN_STATUS_INVALID;
        return hal_status;
    }


    /*
     * Force initial OFF condition.
     */
    Fan_WriteCompare(0U);

    Fan_UpdateCompare(0U);


    /*
     * Update logical state.
     */
    fan_data.state = FAN_OFF;
    fan_data.status = FAN_STATUS_OFF;


    return HAL_OK;
}


/**
 * @brief Stop PWM generation.
 */
HAL_StatusTypeDef Fan_Stop(void)
{
    HAL_StatusTypeDef hal_status;

    /*
     * Check initialization.
     */
    if (!Fan_IsConfigurationValid())
    {
        fan_data.status = FAN_STATUS_NOT_INITIALIZED;
        return HAL_ERROR;
    }

    /*
     * First force PWM to zero.
     */
    Fan_WriteCompare(0U);
    Fan_UpdateCompare(0U);


    /*
     * Update logical state.
     */
    fan_data.state = FAN_OFF;
    fan_data.status = FAN_STATUS_OFF;


    /*
     * Stop PWM peripheral output.
     */
    hal_status =
        HAL_TIM_PWM_Stop(fan_htim, fan_channel);

    if (hal_status != HAL_OK)
    {
        fan_data.status = FAN_STATUS_INVALID;
        return hal_status;
    }

    return HAL_OK;
}


/**
 * @brief Turn fan ON.
 */
HAL_StatusTypeDef Fan_On(void)
{
    uint32_t compare;

    /*
     * Check initialization.
     */
    if (!Fan_IsConfigurationValid())
    {
        fan_data.status = FAN_STATUS_NOT_INITIALIZED;
        return HAL_ERROR;
    }


    /*
     * A speed of 0% means there is no valid ON command.
     * Do NOT automatically change 0% to 100%.
     * The application controls the requested speed.
     */
    if (fan_data.speed_percent == 0U)
    {
        fan_data.state = FAN_OFF;
        fan_data.status = FAN_STATUS_OFF;

        Fan_WriteCompare(0U);
        Fan_UpdateCompare(0U);

        return HAL_OK;
    }

    /*
     * Convert requested speed to PWM compare value.
     */
    compare = Fan_SpeedToCompare(fan_data.speed_percent);

    /*
     * Apply PWM value.
     */
    Fan_WriteCompare(compare);


    /*
     * Update internal data.
     */
    Fan_UpdateCompare(compare);

    fan_data.state = FAN_ON;
    fan_data.status = FAN_STATUS_ON;

    return HAL_OK;
}


/**
 * @brief Turn fan OFF.
 */
HAL_StatusTypeDef Fan_Off(void)
{
    /*
     * Check initialization.
     */
    if (!Fan_IsConfigurationValid())
    {
        fan_data.status = FAN_STATUS_NOT_INITIALIZED;
        return HAL_ERROR;
    }

    /*
     * Force PWM duty cycle to zero.
     */
    Fan_WriteCompare(0U);
    Fan_UpdateCompare(0U);


    /*
     * Update logical state.
     * IMPORTANT:
     * The requested speed percentage is preserved.
     * Example:
     * Fan_SetSpeed(50);
     * Fan_Off();
     * Fan_On();
     * The fan returns to 50%.
     */
    fan_data.state = FAN_OFF;
    fan_data.status = FAN_STATUS_OFF;


    return HAL_OK;
}


/**
 * @brief Set fan speed.
 */
HAL_StatusTypeDef Fan_SetSpeed(uint8_t speed_percent)
{
    uint32_t compare;

    /*
     * Check initialization.
     */
    if (!Fan_IsConfigurationValid())
    {
        fan_data.status = FAN_STATUS_NOT_INITIALIZED;
        return HAL_ERROR;
    }

    /*
     * Validate speed range.
     */
    if (speed_percent > FAN_MAX_SPEED_PERCENT)
    {
        fan_data.status = FAN_STATUS_INVALID;
        return HAL_ERROR;
    }

    /*
     * Store requested speed.
     */
    fan_data.speed_percent = speed_percent;

    /*
     * Calculate corresponding PWM compare.
     */
    compare =
        Fan_SpeedToCompare(speed_percent);

    /*
     * Apply new PWM command.
     * This is intentionally applied immediately.
     */
    Fan_WriteCompare(compare);
    Fan_UpdateCompare(compare);

    /*
     * Speed 0% means OFF.
     */
    if (speed_percent == FAN_MIN_SPEED_PERCENT)
    {
        fan_data.state = FAN_OFF;
        fan_data.status = FAN_STATUS_OFF;
    }
    else
    {
        /*
         * A non-zero speed means the fan is commanded ON.
         */
        fan_data.state = FAN_ON;
        fan_data.status = FAN_STATUS_ON;
    }

    return HAL_OK;
}


/**
 * @brief Get current fan speed.
 */
uint8_t Fan_GetSpeed(void)
{
    return fan_data.speed_percent;
}


/**
 * @brief Get current fan state.
 */
Fan_State_t Fan_GetState(void)
{
    return fan_data.state;
}


/**
 * @brief Get current fan status.
 */
Fan_Status_t Fan_GetStatus(void)
{
    return fan_data.status;
}


/**
 * @brief Get complete fan data.
 */
Fan_Data_t Fan_GetData(void)
{
    return fan_data;
}


/**
 * @brief Check whether fan is ON.
 */
bool Fan_IsOn(void)
{
    return(fan_data.state == FAN_ON);
}


/**
 * @brief Check whether fan is OFF.
 */
bool Fan_IsOff(void)
{
    return(fan_data.state == FAN_OFF);
}


/**
 * @brief Check whether fan driver is initialized.
 */
bool Fan_IsInitialized(void)
{
    return(fan_htim != NULL);
}


/**
 * @brief Force fan into safe OFF state.
 */
void Fan_SafeOff(void)
{
    /*
     * If timer has not been configured,
     * only update software state.
     */
    if (fan_htim == NULL)
    {
        fan_data.state = FAN_OFF;
        fan_data.status = FAN_STATUS_NOT_INITIALIZED;
        fan_data.compare_value = 0U;
        return;
    }


    /*
     * Force PWM to zero.
     */
    Fan_WriteCompare(0U);
    Fan_UpdateCompare(0U);

    /*
     * Update logical state.
     */
    fan_data.state = FAN_OFF;
    fan_data.status = FAN_STATUS_OFF;
}