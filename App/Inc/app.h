/**
 * @file    app.h
 * @brief   Smart Greenhouse application layer.
 * @author  Fatemeh Moghadasian
 * @version 3.0
 */

#ifndef APP_H
#define APP_H


#include "main.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#include "cli.h"


/* Application configuration */

/*
 * Temperature setpoint limits.
 */
#define APP_DEFAULT_SETPOINT_C          (25.0f)
#define APP_MIN_SETPOINT_C              (0.0f)
#define APP_MAX_SETPOINT_C              (50.0f)

/*
 * Automatic control hysteresis.
 * Example:
 * Setpoint = 25 C
 * Heater ON below 24 C
 * Heater OFF at/above 24 C
 * Fan ON above 26 C
 * Fan OFF at/below 26 C
 */
#define APP_TEMPERATURE_HYSTERESIS_C    (1.0f)

/*
 * Warning limits relative to setpoint.
 * Example:
 * Setpoint = 25 C
 * Warning below 20 C
 * Warning above 30 C
 */
#define APP_WARNING_LOW_MARGIN_C        (5.0f)
#define APP_WARNING_HIGH_MARGIN_C       (5.0f)

/*
 * Time allowed for the LM35/ADC subsystem
 * to produce a valid measurement after startup.
 */
#define APP_SENSOR_STARTUP_GRACE_MS     (2000U)

/*
 * Button handling.
 */
#define APP_BUTTON_DEBOUNCE_MS          (200U)
#define APP_BUTTON_POLL_PERIOD_MS       (50U)

/*
 * LCD refresh period.
 */
#define APP_LCD_UPDATE_PERIOD_MS        (500U)

/*
 * Fan control.
 */
#define APP_HIGH_TEMP_FAN_PERCENT       (75U)
#define APP_SENSOR_FAULT_FAN_PERCENT    (100U)


/* Application mode*/

/*
 * Mode represents the USER CONTROL MODE.
 * IMPORTANT:
 * WARNING and FAULT are NOT modes.
 * They are system states.
 */
typedef enum
{
    APP_MODE_AUTO = 0U,
    APP_MODE_MANUAL
} App_Mode_t;


/* Application system state */

typedef enum
{
    APP_STATE_INIT = 0U,
    APP_STATE_NORMAL,
    APP_STATE_WARNING,
    APP_STATE_SENSOR_FAULT,
    APP_STATE_ERROR
} App_SystemState_t;


/* Button events */

typedef enum
{
    APP_BUTTON_NONE = 0U,
    APP_BUTTON_SETPOINT_UP,
    APP_BUTTON_SETPOINT_DOWN,
    APP_BUTTON_MODE,
    APP_BUTTON_MUTE,
    APP_BUTTON_RESET
} App_ButtonEvent_t;


/* Application settings */

typedef struct
{
    float setpoint_c;
    App_Mode_t mode;
} App_Settings_t;


/* Optional storage abstraction */

typedef struct
{
    bool (*Load)(App_Settings_t *settings);
    bool (*Save)(const App_Settings_t *settings);
} App_StorageCallbacks_t;


/* Optional hardware callbacks */

typedef struct
{
    App_ButtonEvent_t (*GetButtonEvent)(void);
    void (*BuzzerOn)(void);
    void (*BuzzerOff)(void);
} App_HardwareCallbacks_t;


/* Runtime application data */

typedef struct
{
    float temperature_c;
    float sensor_voltage_v;
    uint16_t adc_raw;
    bool sensor_valid;
    bool heater_on;
    uint8_t fan_speed_percent;
    float setpoint_c;
    App_Mode_t mode;
    App_SystemState_t state;
} App_Data_t;


/* Public API */

/**
 * @brief Initialize the application layer.
 * This function initializes application-owned drivers and
 * registers the CLI callbacks.
 */
HAL_StatusTypeDef App_Init(
    ADC_HandleTypeDef *hadc,
    TIM_HandleTypeDef *htim_fan,
    UART_HandleTypeDef *huart,
    IWDG_HandleTypeDef *hiwdg,
    TIM_HandleTypeDef *htim_adc,
    const App_StorageCallbacks_t *storage,
    const App_HardwareCallbacks_t *hardware
);


/**
 * @brief Start the application runtime.
 * Starts:
 *  - Fan PWM
 *  - LM35 ADC/DMA acquisition
 *  - ADC trigger timer
 */
HAL_StatusTypeDef App_Start(void);


/**
 * @brief Execute one application cycle.
 * This function must be called continuously from the
 * main application loop.
 */
void App_Process(void);


/**
 * @brief Return a snapshot of the current application data.
 */
App_Data_t App_GetData(void);


/**
 * @brief Return current user control mode.
 */
App_Mode_t App_GetMode(void);


/**
 * @brief Set user control mode.
 */
bool App_SetMode(App_Mode_t mode);


/**
 * @brief Return current temperature setpoint.
 */
float App_GetSetpoint(void);


/**
 * @brief Set temperature setpoint.
 */
bool App_SetSetpoint(float temperature_c);


/**
 * @brief Force all actuators into a safe state.
 * Heater OFF
 * Fan OFF
 * Buzzer OFF
 */
void App_SafeState(void);


/**
 * @brief Load application settings.
 */
bool App_LoadSettings(void);


/**
 * @brief Save application settings.
 */
bool App_SaveSettings(void);


/* CLI callbacks */

void App_CLI_GetSnapshot(CLI_SystemSnapshot_t *snapshot);

bool App_CLI_SetSetpoint(float temperature_c);

bool App_CLI_SetFanPercent(uint8_t percent);

bool App_CLI_FanOn(void);

bool App_CLI_FanOff(void);

bool App_CLI_HeaterOn(void);

bool App_CLI_HeaterOff(void);

bool App_CLI_SetMode(CLI_Mode_t mode);


#endif /* APP_H */