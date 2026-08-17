/**
 * @file    app.c
 * @brief   Smart Greenhouse application layer implementation.
 *
 * @author  Fatemeh Moghadasian
 * @version 3.0
 */

#include "app.h"

#include "main.h"
#include "sensor_lm35.h"
#include "heater.h"
#include "fan.h"
#include "lcd.h"

#include <stdio.h>
#include <string.h>


/* Private hardware handles */
static ADC_HandleTypeDef *app_hadc = NULL;
static TIM_HandleTypeDef *app_htim_fan = NULL;
static UART_HandleTypeDef *app_huart = NULL;
static IWDG_HandleTypeDef *app_hiwdg = NULL;
static TIM_HandleTypeDef *app_htim_adc = NULL;

/* Application lifecycle */
static bool app_initialized = false;
static bool app_started = false;


/* Application timing */
static uint32_t app_start_time = 0U;
static uint32_t app_last_lcd_update = 0U;
static uint32_t app_last_button_poll = 0U;
static uint32_t app_last_button_event = 0U;


/* Application settings */

static App_Settings_t app_settings =
{
    .setpoint_c = APP_DEFAULT_SETPOINT_C,
    .mode = APP_MODE_AUTO
};


/* Runtime data */

static App_Data_t app_data =
{
    .temperature_c = 0.0f,
    .sensor_voltage_v = 0.0f,
    .adc_raw = 0U,
    .sensor_valid = false,
    .heater_on = false,
    .fan_speed_percent = 0U,
    .setpoint_c = APP_DEFAULT_SETPOINT_C,
    .mode = APP_MODE_AUTO,
    .state = APP_STATE_INIT
};


/* Optional callbacks */

static App_StorageCallbacks_t app_storage;
static App_HardwareCallbacks_t app_hardware;


/* Buzzer */

static bool app_buzzer_muted = false;


/* Private function prototypes*/
static void App_ProcessSensor(void);
static void App_UpdateControl(void);
static void App_HandleAutomaticControl(void);
static void App_HandleWarning(void);
static void App_EnterSensorFault(void);
static void App_LeaveSensorFault(void);
static void App_UpdateWarningState(void);
static void App_UpdateLCD(void);
static void App_ProcessButtons(void);
static void App_ProcessButtonEvent(App_ButtonEvent_t event);
static void App_HandleModeButton(void);
static void App_ApplyHeater(bool on);
static void App_ApplyFan(uint8_t percent);
static void App_RefreshWatchdog(void);
static App_ButtonEvent_t App_ReadButtonEvent(void);
static void App_BuzzerOn(void);
static void App_BuzzerOff(void);
static float App_GetWarningLowLimit(void);
static float App_GetWarningHighLimit(void);
static const char *App_ModeToString(App_Mode_t mode);
static const char *App_StateToString(App_SystemState_t state);


/* App_Init */

HAL_StatusTypeDef App_Init(
    ADC_HandleTypeDef *hadc,
    TIM_HandleTypeDef *htim_fan,
    UART_HandleTypeDef *huart,
    IWDG_HandleTypeDef *hiwdg,
    TIM_HandleTypeDef *htim_adc,
    const App_StorageCallbacks_t *storage,
    const App_HardwareCallbacks_t *hardware
)
{
    CLI_Callbacks_t cli_callbacks;

    /*
     * Validate required handles
     */

    if (hadc == NULL)
    {
        return HAL_ERROR;
    }

    if (htim_fan == NULL)
    {
        return HAL_ERROR;
    }

    if (huart == NULL)
    {
        return HAL_ERROR;
    }

    if (htim_adc == NULL)
    {
        return HAL_ERROR;
    }

    /*
     * Store peripheral handles
     */

    app_hadc = hadc;
    app_htim_fan = htim_fan;
    app_huart = huart;
    app_hiwdg = hiwdg;
    app_htim_adc = htim_adc;


    /*
     * Reset callback structures
     */

    memset(&app_storage, 0, sizeof(app_storage));

    memset(&app_hardware,0, sizeof(app_hardware));


    /*
     * Copy optional callbacks
     */

    if (storage != NULL)
    {
        app_storage = *storage;
    }

    if (hardware != NULL)
    {
        app_hardware = *hardware;
    }

    /*
     * Application-owned hardware callbacks
     */

    app_hardware.GetButtonEvent = App_ReadButtonEvent;
    app_hardware.BuzzerOn = App_BuzzerOn;
    app_hardware.BuzzerOff = App_BuzzerOff;

    /*
     * Reset application state
     */

    memset(&app_data, 0,sizeof(app_data));

    app_data.temperature_c = 0.0f;
    app_data.sensor_voltage_v = 0.0f;
    app_data.adc_raw = 0U;

    app_data.sensor_valid = false;
    app_data.heater_on = false;
    app_data.fan_speed_percent = 0U;

    app_data.setpoint_c = APP_DEFAULT_SETPOINT_C;

    app_data.mode = APP_MODE_AUTO;

    app_data.state = APP_STATE_INIT;

    /*
     * Reset settings
     */

    app_settings.setpoint_c = APP_DEFAULT_SETPOINT_C;

    app_settings.mode = APP_MODE_AUTO;

    /*
     * Reset runtime flags
     */

    app_initialized = false;
    app_start_time = 0U;
    app_last_lcd_update = 0U;
    app_last_button_poll = 0U;
    app_last_button_event = 0U;
    app_buzzer_muted = false;

    /*
     * Initialize LM35 sensor driver
     */

    if (Sensor_Init(app_hadc) != HAL_OK)
    {
        app_data.state = APP_STATE_ERROR;

        return HAL_ERROR;
    }

    /*
     * Initialize heater driver
     */

    Heater_Init();

    /*
     * Initialize fan driver
     */

    if (Fan_Init(app_htim_fan, TIM_CHANNEL_1) != HAL_OK)
    {
        app_data.state = APP_STATE_ERROR;
        return HAL_ERROR;
    }

    /*
     * Initialize LCD
     */

    LCD_Init();


    /*
     * Load settings
     */

    (void)App_LoadSettings();


    /*
     * Prepare CLI callbacks
     */

    memset(&cli_callbacks, 0, sizeof(cli_callbacks));

    cli_callbacks.GetSnapshot = App_CLI_GetSnapshot;
    cli_callbacks.SetSetpoint = App_CLI_SetSetpoint;
    cli_callbacks.SetFanPercent = App_CLI_SetFanPercent;
    cli_callbacks.FanOn = App_CLI_FanOn;
    cli_callbacks.FanOff = App_CLI_FanOff;
    cli_callbacks.HeaterOn = App_CLI_HeaterOn;
    cli_callbacks.HeaterOff = App_CLI_HeaterOff;
    cli_callbacks.SetMode = App_CLI_SetMode;

    /*
     * Initialize CLI
     */

    if (CLI_Init(app_huart, &cli_callbacks) != HAL_OK)
    {
        app_data.state = APP_STATE_ERROR;
        return HAL_ERROR;
    }


    /*
     * Safe initial hardware state
     */

    Heater_SafeOff();

    Fan_SafeOff();

    App_BuzzerOff();


    /*
     * Initialization successful
     */

    app_initialized = true;

    app_data.state = APP_STATE_INIT;

    return HAL_OK;
}

/*  App_Start */

HAL_StatusTypeDef App_Start(void)
{
    HAL_StatusTypeDef status;
    uint32_t now;

    /* Validate lifecycle  */

    if (!app_initialized)
    {
        return HAL_ERROR;
    }

    /*
     * Prevent accidental double-start.
     */
    if (app_started)
    {
        return HAL_OK;
    }


    /* Start fan PWM */
    status = Fan_Start();

    if (status != HAL_OK)
    {
        app_data.state = APP_STATE_ERROR;
        App_SafeState();
        return HAL_ERROR;
    }


    /* Arm ADC/DMA acquisition */
    status = Sensor_Start();

    if (status != HAL_OK)
    {
        app_data.state = APP_STATE_ERROR;
        App_SafeState();
        return HAL_ERROR;
    }


    status = HAL_TIM_Base_Start(app_htim_adc);

    if (status != HAL_OK)
    {
        app_data.state = APP_STATE_ERROR;
        App_SafeState();
        return HAL_ERROR;
    }


    /* Runtime timing */
    now = HAL_GetTick();

    app_start_time = now;

    app_last_lcd_update = now;

    app_last_button_poll = now;

    /*
     * Set this to a value older than the debounce period so
     * the first valid button press is not ignored.
     */
    app_last_button_event = now - APP_BUTTON_DEBOUNCE_MS;


    app_buzzer_muted = false;


    /* Application state */
    app_data.state = APP_STATE_INIT;
    app_data.sensor_valid = false;
    app_data.heater_on = false;
    app_data.fan_speed_percent = 0U;
    app_started = true;

    
    return HAL_OK;
}


/* App_Process */
void App_Process(void)
{
    uint32_t now;

    /*
     * Application must be initialized and started.
     */
    if ((!app_initialized) || (!app_started))
    {
        return;
    }


    now = HAL_GetTick();

    /* 1. CLI  */
    CLI_Process();

    /* 2. Sensor driver */
    Sensor_Process();

    /* 3. Update application sensor data */
    App_ProcessSensor();

    /* 4. Execute control state machine */
    App_UpdateControl();

    /* 5. Buttons */
    if ((now - app_last_button_poll) >= APP_BUTTON_POLL_PERIOD_MS)
    {
        app_last_button_poll = now;
        App_ProcessButtons();
    }

    /* 6. LCD  */
    if ((now - app_last_lcd_update) >= APP_LCD_UPDATE_PERIOD_MS)
    {
        app_last_lcd_update = now;
        App_UpdateLCD();
    }

    /* 7. Watchdog */
    App_RefreshWatchdog();
}


/* Sensor processing */
static void App_ProcessSensor(void)
{
    Sensor_Data_t sensor;


    sensor = Sensor_GetData();
    app_data.temperature_c = sensor.temperature_c;
    app_data.sensor_voltage_v = sensor.voltage_v;
    app_data.adc_raw = sensor.adc_raw;
    app_data.sensor_valid = (sensor.status == SENSOR_STATUS_OK);
}

/*  Main control state machine */
static void App_UpdateControl(void)
{
    uint32_t elapsed;
    elapsed = HAL_GetTick() - app_start_time;

    /* Startup grace period */
    if (elapsed < APP_SENSOR_STARTUP_GRACE_MS)
    {
        App_ApplyHeater(false);
        App_ApplyFan(0U);
        app_data.state = APP_STATE_INIT;
        return;
    }

    /* Sensor fault has highest priority */
    if (!app_data.sensor_valid)
    {
        App_EnterSensorFault();
        return;
    }


    /* Sensor recovered */
    if (app_data.state == APP_STATE_SENSOR_FAULT)
    {
        App_LeaveSensorFault();
    }


    /* MANUAL MODE */
    if (app_settings.mode == APP_MODE_MANUAL)
    {
        app_data.mode = APP_MODE_MANUAL;
        app_data.state = APP_STATE_NORMAL;
        App_BuzzerOff();
        return;
    }


    /* AUTO MODE */
    app_data.mode = APP_MODE_AUTO;


    /* Automatic temperature control */
    App_HandleAutomaticControl();
}


/* Warning state calculation */

static void App_UpdateWarningState(void)
{
    float low_limit;
    float high_limit;


    low_limit = App_GetWarningLowLimit();
    high_limit = App_GetWarningHighLimit();

    if ((app_data.temperature_c < low_limit) || (app_data.temperature_c > high_limit))
    {
        app_data.state = APP_STATE_WARNING;
        return;
    }


    /*
     * Temperature is inside the warning region.
     */
    if (app_data.state == APP_STATE_WARNING)
    {
        app_data.state = APP_STATE_NORMAL;
        App_BuzzerOff();
    }
}


/* Automatic temperature control */
static void App_HandleAutomaticControl(void)
{
    float low_limit;
    float high_limit;

    low_limit = app_settings.setpoint_c - APP_TEMPERATURE_HYSTERESIS_C;
    high_limit = app_settings.setpoint_c + APP_TEMPERATURE_HYSTERESIS_C;

    /* Temperature below control band  */

    if (app_data.temperature_c < low_limit)
    {
        /*
         * Heater operation requires fan OFF.
         */
        App_ApplyFan(0U);

        App_ApplyHeater(true);

        app_data.state = APP_STATE_NORMAL;

        return;
    }


    /* Temperature above control band */

    if (app_data.temperature_c > high_limit)
    {
        /*
         * Heater OFF.
         */
        App_ApplyHeater(false);

        /*
         * Cooling fan.
         */
        App_ApplyFan(APP_HIGH_TEMP_FAN_PERCENT);

        app_data.state = APP_STATE_NORMAL;

        return;
    }


    /* Temperature inside hysteresis band */

    App_ApplyHeater(false);

    App_ApplyFan(0U);

    app_data.state = APP_STATE_NORMAL;
}


/* Warning handling */

static void App_HandleWarning(void)
{
    /*
     * WARNING policy:
     * Heater = OFF
     * Fan    = 100 %
     * Buzzer = ON unless muted
     * IMPORTANT:
     * Warning is a SYSTEM STATE
     */
    App_ApplyHeater(false);

    App_ApplyFan(100U);


    if (!app_buzzer_muted)
    {
        App_BuzzerOn();
    }
}


/* Sensor fault handling */

static void App_EnterSensorFault(void)
{
    /*
     * SENSOR FAULT safety policy:
     * Heater = OFF
     * Fan    = 100 %
     * Buzzer = ON unless muted
     */
    app_data.state = APP_STATE_SENSOR_FAULT;


    /*
     * Keep user-selected mode intact.
     * Do NOT change app_settings.mode.
     */
    app_data.mode = app_settings.mode;


    App_ApplyHeater(false);

    App_ApplyFan( APP_SENSOR_FAULT_FAN_PERCENT);


    if (!app_buzzer_muted)
    {
        App_BuzzerOn();
    }
}


/* Sensor fault recovery */

static void App_LeaveSensorFault(void)
{
    App_BuzzerOff();

    app_buzzer_muted = false;

    app_data.state = APP_STATE_NORMAL;

    app_data.mode = app_settings.mode;
}


/* Heater control */

static void App_ApplyHeater(bool on)
{
    if (on)
    {
        /*
         * Safety interlock:
         * Fan OFF before heater ON.
         */
        if (Fan_Off() == HAL_OK)
        {
            app_data.fan_speed_percent = Fan_GetSpeed();
        }


        Heater_On();

        app_data.heater_on = Heater_IsOn();

        return;
    }


    Heater_Off();

    app_data.heater_on = false;
}


/* Fan control */

static void App_ApplyFan(uint8_t percent)
{
    if (percent > 100U)
    {
        percent = 100U;
    }


    if (percent == 0U)
    {
        if (Fan_Off() == HAL_OK)
        {
            app_data.fan_speed_percent = 0U;
        }

        return;
    }

    if (Fan_SetSpeed(percent) == HAL_OK)
    {
        app_data.fan_speed_percent = Fan_GetSpeed();
    }
}


/* LCD */

static void App_UpdateLCD(void)
{
    char line1[17];
    char line2[17];

    memset(line1, ' ', 16U);
    memset(line2, ' ', 16U);

    line1[16] = '\0';
    line2[16] = '\0';


    /* Sensor fault */

    if (app_data.state == APP_STATE_SENSOR_FAULT)
    {
        snprintf(line1, sizeof(line1), "SENSOR FAULT");

        snprintf(line2, sizeof(line2), "FAN:%3u%%",(unsigned int)app_data.fan_speed_percent);
    }


    /* Manual mode */

    else if (app_data.mode == APP_MODE_MANUAL)
    {
        snprintf(line1, sizeof(line1),"T:%4.1f SP:%4.1f", app_data.temperature_c, app_data.setpoint_c);

        if (app_data.heater_on)
        {
            snprintf(line2, sizeof(line2), "MAN H:ON");
        }
        else if (app_data.fan_speed_percent > 0U)
        {
            snprintf(line2, sizeof(line2), "MAN F:%3u%%",(unsigned int)app_data.fan_speed_percent);
        }
        else
        {
            snprintf(line2, sizeof(line2), "MANUAL IDLE");
        }
    }


    /* Auto mode */

    else
    {
        snprintf(line1, sizeof(line1), "T:%4.1f SP:%4.1f", app_data.temperature_c, app_data.setpoint_c);

        if (app_data.heater_on)
        {
            snprintf(line2, sizeof(line2), "HEATER ON");
        }
        else if (app_data.fan_speed_percent > 0U)
        {
            snprintf(line2, sizeof(line2), "FAN:%3u%%", (unsigned int)app_data.fan_speed_percent);
        }
        else
        {
            snprintf(line2, sizeof(line2),"SYSTEM OK");
        }
    }


    /* LCD output */

    LCD_SetCursor(0U, 0U);
    LCD_Print(line1);

    LCD_SetCursor(1U, 0U);
    LCD_Print("                ");

    LCD_SetCursor(1U, 0U);
    LCD_Print(line2);
}

/* Button processing */

static void App_ProcessButtons(void)
{
    App_ButtonEvent_t event;

    if (app_hardware.GetButtonEvent == NULL)
    {
        return;
    }

    event = app_hardware.GetButtonEvent();

    if (event == APP_BUTTON_NONE)
    {
        return;
    }


    if ((HAL_GetTick() - app_last_button_event) < APP_BUTTON_DEBOUNCE_MS)
    {
        return;
    }


    app_last_button_event = HAL_GetTick();

    App_ProcessButtonEvent(event);
}


/* Button dispatcher */

static void App_ProcessButtonEvent(App_ButtonEvent_t event)
{
    switch (event)
    {
        case APP_BUTTON_SETPOINT_UP:
            (void)App_SetSetpoint(app_settings.setpoint_c + 1.0f);
            break;

        case APP_BUTTON_SETPOINT_DOWN:
            (void)App_SetSetpoint(app_settings.setpoint_c - 1.0f);
            break;

        case APP_BUTTON_MODE:
            App_HandleModeButton();
            break;

        case APP_BUTTON_MUTE:
            app_buzzer_muted =true;
            App_BuzzerOff();
            break;


        case APP_BUTTON_RESET:

            /*
             * Reset only clears the actuator state and
             * alarm mute.
             *
             * The next control cycle will determine the
             * correct safety state again.
             */
            App_SafeState();
            app_buzzer_muted =false;
            app_data.state = APP_STATE_INIT;
            app_start_time = HAL_GetTick();
            break;

        case APP_BUTTON_NONE:

        default:

            break;
    }
}


/* Mode button */

static void App_HandleModeButton(void)
{
    if (app_settings.mode == APP_MODE_AUTO)
    {
        (void)App_SetMode(APP_MODE_MANUAL);
    }
    else
    {
        (void)App_SetMode(APP_MODE_AUTO);
    }
}


/* Setpoint */

bool App_SetSetpoint(float temperature_c)
{
    if (temperature_c < APP_MIN_SETPOINT_C)
    {
        return false;
    }


    if (temperature_c > APP_MAX_SETPOINT_C)
    {
        return false;
    }

    app_settings.setpoint_c = temperature_c;

    app_data.setpoint_c = temperature_c;


    /*
     * Persistence is optional.
     */
    (void)App_SaveSettings();

    return true;
}


/* Mode */

bool App_SetMode(App_Mode_t mode)
{
    if ((mode != APP_MODE_AUTO) &&
        (mode != APP_MODE_MANUAL))
    {
        return false;
    }

    /*
     * Change application mode.
     */
    app_settings.mode = mode;

    app_data.mode = mode;


    if (mode == APP_MODE_AUTO)
    {
        App_ApplyHeater(false);
        App_ApplyFan(0U);
    }


    else
    {
        App_ApplyHeater(false);
        App_ApplyFan(0U);
    }


    /*
     * Persistence is optional.
     */
    (void)App_SaveSettings();


    return true;
}


/* Load settings */

bool App_LoadSettings(void)
{
    App_Settings_t settings;


    if (app_storage.Load == NULL)
    {
        app_settings.setpoint_c = APP_DEFAULT_SETPOINT_C;

        app_settings.mode = APP_MODE_AUTO;

        app_data.setpoint_c = APP_DEFAULT_SETPOINT_C;

        app_data.mode = APP_MODE_AUTO;

        return false;
    }


    memset(&settings, 0, sizeof(settings));


    if (!app_storage.Load(&settings))
    {
        return false;
    }


    /* Validate setpoint */
    if ((settings.setpoint_c < APP_MIN_SETPOINT_C) ||(settings.setpoint_c > APP_MAX_SETPOINT_C))
    {
        return false;
    }


    /* Validate mode */
    if ((settings.mode != APP_MODE_AUTO) &&(settings.mode != APP_MODE_MANUAL))
    {
        return false;
    }


    /* Commit settings */

    app_settings = settings;
    app_data.setpoint_c = app_settings.setpoint_c;
    app_data.mode = app_settings.mode;

    return true;
}


/* Save settings */

bool App_SaveSettings(void)
{
    if (app_storage.Save == NULL)
    {
        return false;
    }

    return app_storage.Save(&app_settings);
}


/* Safe state */

void App_SafeState(void)
{
    Heater_SafeOff();

    app_data.heater_on = false;

    Fan_SafeOff();

    app_data.fan_speed_percent = 0U;

    App_BuzzerOff();
}


/* Runtime getters */

App_Data_t App_GetData(void)
{
    return app_data;
}


App_Mode_t App_GetMode(void)
{
    return app_settings.mode;
}


float App_GetSetpoint(void)
{
    return app_settings.setpoint_c;
}


/* CLI snapshot */

void App_CLI_GetSnapshot(CLI_SystemSnapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }


    memset(snapshot, 0, sizeof(*snapshot));

    snapshot->sensor_valid = app_data.sensor_valid;

    snapshot->temperature_c = app_data.temperature_c;

    snapshot->adc_raw = app_data.adc_raw;

    snapshot->sensor_voltage_v = app_data.sensor_voltage_v;

    snapshot->setpoint_c = app_settings.setpoint_c;

    snapshot->heater_on = app_data.heater_on;

    snapshot->fan_speed_percent = app_data.fan_speed_percent;


    /* Mode */

    switch (app_settings.mode)
    {
        case APP_MODE_AUTO:
            snapshot->mode = CLI_MODE_AUTO;
            break;

        case APP_MODE_MANUAL:
            snapshot->mode = CLI_MODE_MANUAL;
            break;


        default:
            snapshot->mode = CLI_MODE_UNKNOWN;
            break;
    }


    snapshot->mode_name = App_ModeToString(app_settings.mode);


    /* System state */

    switch (app_data.state)
    {
        case APP_STATE_INIT:
            snapshot->system_state = CLI_SYSTEM_STATE_INIT;
            break;


        case APP_STATE_NORMAL:
            snapshot->system_state = CLI_SYSTEM_STATE_NORMAL;
            break;


        case APP_STATE_WARNING:
            snapshot->system_state = CLI_SYSTEM_STATE_WARNING;
            break;


        case APP_STATE_SENSOR_FAULT:
            snapshot->system_state = CLI_SYSTEM_STATE_SENSOR_FAULT;
            break;


        case APP_STATE_ERROR:

        default:
            snapshot->system_state = CLI_SYSTEM_STATE_ERROR;
            break;
    }


    snapshot->system_state_name = App_StateToString(app_data.state);
}


/* CLI setpoint */

bool App_CLI_SetSetpoint(float temperature_c)
{
    return App_SetSetpoint(temperature_c);
}


/* CLI fan percentage */

bool App_CLI_SetFanPercent(uint8_t percent)
{
    if (app_settings.mode != APP_MODE_MANUAL)
    {
        return false;
    }

    if (percent > 100U)
    {
        percent = 100U;
    }

    /*
     * Heater and fan are mutually exclusive.
     */
    if (percent > 0U)
    {
        Heater_Off();
        app_data.heater_on = false;
    }

    if (Fan_SetSpeed(percent) != HAL_OK)
    {
        return false;
    }

    app_data.fan_speed_percent = Fan_GetSpeed();

    return true;
}


/* CLI fan ON */

bool App_CLI_FanOn(void)
{
    if (app_settings.mode != APP_MODE_MANUAL)
    {
        return false;
    }


    /*
     * Fan ON requires heater OFF.
     */
    Heater_Off();

    app_data.heater_on = false;

    if (Fan_On() != HAL_OK)
    {
        return false;
    }

    app_data.fan_speed_percent = Fan_GetSpeed();

    return true;
}


/* CLI fan OFF */

bool App_CLI_FanOff(void)
{
    if (app_settings.mode != APP_MODE_MANUAL)
    {
        return false;
    }

    if (Fan_Off() != HAL_OK)
    {
        return false;
    }

    app_data.fan_speed_percent = 0U;

    return true;
}


/* CLI heater ON */

bool App_CLI_HeaterOn(void)
{
    if (app_settings.mode != APP_MODE_MANUAL)
    {
        return false;
    }

    /*
     * Safety interlock:
     * Heater ON => Fan OFF.
     */
    if (Fan_Off() != HAL_OK)
    {
        return false;
    }

    app_data.fan_speed_percent = Fan_GetSpeed();

    Heater_On();

    app_data.heater_on = Heater_IsOn();

    return true;
}


/* CLI heater OFF */

bool App_CLI_HeaterOff(void)
{
    if (app_settings.mode != APP_MODE_MANUAL)
    {
        return false;
    }

    Heater_Off();

    app_data.heater_on = false;

    if (Fan_Off() != HAL_OK)
    {
        return false;
    }

    app_data.fan_speed_percent = 0U;

    return true;
}


/* CLI mode */

bool App_CLI_SetMode(CLI_Mode_t mode)
{
    switch (mode)
    {
        case CLI_MODE_AUTO:
            return App_SetMode(APP_MODE_AUTO);

        case CLI_MODE_MANUAL:
            return App_SetMode(APP_MODE_MANUAL);

        default:
            return false;
    }
}


/* Mode string */

static const char *App_ModeToString(App_Mode_t mode)
{
    switch (mode)
    {
        case APP_MODE_AUTO:
            return "AUTO";

        case APP_MODE_MANUAL:
            return "MANUAL";

        default:
            return "UNKNOWN";
    }
}


/* State string */
static const char *App_StateToString(App_SystemState_t state)
{
    switch (state)
    {
        case APP_STATE_INIT:
            return "INIT";

        case APP_STATE_NORMAL:
            return "NORMAL";


        case APP_STATE_WARNING:
            return "WARNING";


        case APP_STATE_SENSOR_FAULT:
            return "SENSOR_FAULT";


        case APP_STATE_ERROR:
            return "ERROR";


        default:
            return "UNKNOWN";
    }
}


/* Button input */
static App_ButtonEvent_t App_ReadButtonEvent(void)
{

    if (HAL_GPIO_ReadPin(BUTTON_MODE_GPIO_Port,BUTTON_MODE_Pin) == GPIO_PIN_RESET)
    {
        return APP_BUTTON_MODE;
    }

    if (HAL_GPIO_ReadPin(BUTTON_TEMP_UP_GPIO_Port,BUTTON_TEMP_UP_Pin) == GPIO_PIN_RESET)
    {
        return APP_BUTTON_SETPOINT_UP;
    }

    if (HAL_GPIO_ReadPin(BUTTON_TEMP_DOWN_GPIO_Port,BUTTON_TEMP_DOWN_Pin) == GPIO_PIN_RESET)
    {
        return APP_BUTTON_SETPOINT_DOWN;
    }

    if (HAL_GPIO_ReadPin(BUTTON_MUTE_GPIO_Port,BUTTON_MUTE_Pin) == GPIO_PIN_RESET)
    {
        return APP_BUTTON_MUTE;
    }

    if (HAL_GPIO_ReadPin(BUTTON_RESET_GPIO_Port,BUTTON_RESET_Pin) == GPIO_PIN_RESET)
    {
        return APP_BUTTON_RESET;
    }


    return APP_BUTTON_NONE;
}


/* Buzzer */

static void App_BuzzerOn(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port,BUZZER_Pin,GPIO_PIN_RESET);
}


static void App_BuzzerOff(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port,BUZZER_Pin,GPIO_PIN_SET);
}


/* Warning limits */

static float App_GetWarningLowLimit(void)
{
    return app_settings.setpoint_c - APP_WARNING_LOW_MARGIN_C;
}

static float App_GetWarningHighLimit(void)
{
    return app_settings.setpoint_c + APP_WARNING_HIGH_MARGIN_C;
}


/* Watchdog */

static void App_RefreshWatchdog(void)
{
    if (app_hiwdg == NULL)
    {
        return;
    }

    (void)HAL_IWDG_Refresh(app_hiwdg);
}