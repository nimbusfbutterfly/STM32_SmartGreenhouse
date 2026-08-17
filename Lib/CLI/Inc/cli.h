/**
 * @file    cli.h
 * @brief   UART Command Line Interface with RX Ring Buffer
 * @author  Fatemeh Moghadasian
 * @version 2.0
 */

#ifndef CLI_H
#define CLI_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>


/* Configuration */

/**
 * @brief Maximum number of characters in one command line.
 * Example:
 *      setpoint 25.5
 */
#define CLI_LINE_BUFFER_SIZE       (128U)


/**
 * @brief Number of bytes in UART RX ring buffer.
 * Must be a power of two for the optimized modulo operation.
 * 256 = 2^8
 */
#define CLI_RX_RING_BUFFER_SIZE    (256U)


/**
 * @brief Maximum number of command tokens.
 * Example:
 *      fan 75
 * tokens:
 *      [0] = "fan"
 *      [1] = "75"
 */
#define CLI_MAX_TOKENS              (8U)


/**
 * @brief UART transmit timeout.
 * CLI transmission is performed outside interrupt context,
 * therefore blocking transmission is acceptable for this
 * project.
 */
#define CLI_TX_TIMEOUT_MS          (100U)


/**
 * @brief Firmware version displayed by "version" command.
 */
#define CLI_FIRMWARE_VERSION       "Smart Greenhouse FW v2.0"


/**
 * @brief Minimum allowed setpoint.
 */
#define CLI_SETPOINT_MIN_C         (0.0f)


/**
 * @brief Maximum allowed setpoint.
 */
#define CLI_SETPOINT_MAX_C         (50.0f)


/* CLI Status */

/**
 * @brief CLI driver status.
 */
typedef enum
{
    CLI_STATUS_NOT_INITIALIZED = 0U,
    CLI_STATUS_OK,
    CLI_STATUS_RX_OVERFLOW,
    CLI_STATUS_UART_ERROR
} CLI_Status_t;


/* System Mode */

/**
 * @brief Application operating mode.
 */
typedef enum
{
    CLI_MODE_AUTO = 0U,
    CLI_MODE_MANUAL,
    CLI_MODE_WARNING,
    CLI_MODE_FAULT,
    CLI_MODE_UNKNOWN
} CLI_Mode_t;


/* System State*/

/**
 * @brief High-level greenhouse state.
 */
typedef enum
{
    CLI_SYSTEM_STATE_INIT = 0U,
    CLI_SYSTEM_STATE_NORMAL,
    CLI_SYSTEM_STATE_WARNING,
    CLI_SYSTEM_STATE_SENSOR_FAULT,
    CLI_SYSTEM_STATE_ERROR
} CLI_SystemState_t;


/* System Snapshot */

/**
 * @brief Snapshot of the current greenhouse state.
 * The application fills this structure when CLI requests
 * "status" or "temperature".
 */
typedef struct
{
    bool sensor_valid;
    float temperature_c;
    float setpoint_c;
    uint16_t adc_raw;
    float sensor_voltage_v;
    bool heater_on;
    uint8_t fan_speed_percent;
    CLI_Mode_t mode;
    CLI_SystemState_t system_state;
    const char *mode_name;
    const char *system_state_name;
} CLI_SystemSnapshot_t;


/* Callback Interface */

/**
 * @brief Application callback interface..
 */
typedef struct
{
    /**
     * @brief Obtain current greenhouse status.
     */
    void (*GetSnapshot)(CLI_SystemSnapshot_t *snapshot);


    /**
     * @brief Change temperature setpoint.
     * @param temperature_c New setpoint.
     * @return
     *      true  -> accepted
     *      false -> rejected
     */
    bool (*SetSetpoint)(float temperature_c);

    /**
     * @brief Set fan speed percentage.
     * @param percent 0..100.
     */
    bool (*SetFanPercent)(uint8_t percent);

    /**
     * @brief Turn fan ON.
     */
    bool (*FanOn)(void);

    /**
     * @brief Turn fan OFF.
     */
    bool (*FanOff)(void);

    /**
     * @brief Turn heater ON.
     */
    bool (*HeaterOn)(void);

    /**
     * @brief Turn heater OFF.
     */
    bool (*HeaterOff)(void);


    /**
     * @brief Change operating mode.
     * This callback is optional.
     * If NULL, the "mode" command is unavailable.
     */
    bool (*SetMode)(CLI_Mode_t mode);
} CLI_Callbacks_t;


/* Function prototypes */


/**
 * @brief Initialize CLI.
 * @param huart UART handle configured by CubeMX.
 * @param callbacks Application callback table.
 * @return HAL_OK if initialization succeeds.
 */
HAL_StatusTypeDef CLI_Init(UART_HandleTypeDef *huart,const CLI_Callbacks_t *callbacks);


/**
 * @brief Process pending CLI input.
 */
void CLI_Process(void);


/**
 * @brief UART receive-complete callback.
 * This function must be called from:
 *      HAL_UART_RxCpltCallback()
 * It executes in interrupt context.
 */
void CLI_UART_RxCpltCallback(void);


/**
 * @brief UART error callback.
 * This function may be called from:
 *      HAL_UART_ErrorCallback()
 */
void CLI_UART_ErrorCallback(void);


/**
 * @brief Get current CLI status.
 */
CLI_Status_t CLI_GetStatus(void);


/**
 * @brief Get number of bytes currently waiting in RX buffer.
 */
uint16_t CLI_GetRxPending(void);


/**
 * @brief Check whether RX ring buffer overflow occurred.
 */
bool CLI_HasRxOverflow(void);


/**
 * @brief Clear RX overflow flag.
 */
void CLI_ClearRxOverflow(void);


/**
 * @brief Send arbitrary text through CLI UART.
 * This function is safe to call from normal application
 * context, but MUST NOT be called from ISR context.
 */
void CLI_Print(const char *text);


/**
 * @brief Send a formatted system status.
 */
void CLI_PrintStatus(void);


#endif /* CLI_H */