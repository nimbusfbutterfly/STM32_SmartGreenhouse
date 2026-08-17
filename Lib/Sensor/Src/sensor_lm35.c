/**
 * @file sensor_lm35.c
 * @brief LM35 temperature sensor driver implementation
 * @details
 * This module implements ADC DMA acquisition and processing 
 * the LM35 temprature sensor.
 * The ADC is triggered by a timer and transfers samples into a
 * Circular DMA buffer.
 * The DMA buffer is divided into two blocks:
 *  +------------+------------+
 *  |  Half 1    |  Half 2    |
 *  +------------+------------+
 * Half-transfer interrupt indicates that Half 1 is ready
 * Transfer-complete interrupt indicates that Half 2 is ready
 * The interrupt callbacks only set event flags. Actual numerical
 * processing is performed outside interrupt context by
 * Sensor_Process() function.
 * @author Fatemeh Moghadasian
 * @version 1.1
 */

#include "sensor_lm35.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>



/* Private Variables */
/**
 * @brief ADC handle supplied by CubeMX
 */
static ADC_HandleTypeDef *sensor_hadc = NULL;

/**
 * @brief ADC DMA circular buffer
 */
static uint16_t sensor_adc_buffer[SENSOR_ADC_BUFFER_SIZE];

/**
 * @brief Pending DMA event
 * This variable is modified from interrupt context
 */
static volatile Sensor_Event_t sensor_event = SENSOR_EVENT_NONE;

/**
 * @brief Indicates that a new measurment has been processed
 */
static volatile bool sensor_data_ready = false;

/**
 * @brief Latest processed sensor data
 */
static Sensor_Data_t sensor_data =
{
    .adc_raw = 0U,
    .voltage_v = 0.0f,
    .temperature_c = 0.0f,
    .status = SENSOR_STATUS_NO_DATA
};

/* Private Function */

/**
 * @brief  Calculate average ADC value
 * @param  buffer Pointer to ADC sample buffer
 * @param  length Number of ADC samples
 * @return Average ADC value
 */
static uint16_t Sensor_CalculateAverage(const uint16_t *buffer,uint16_t length);


/**
 * @brief  Convert ADC digital value to voltage
 * @param  adc_value Averaged ADC value
 * @return ADC voltage in volts */
static float Sensor_ADCToVoltage(uint16_t adc_value);


/**
 * @brief  Convert LM35 output voltage to temperature
 * @param  voltage_v Output voltage from LM35
 * @return Temperature in degrees Celsius */
static float Sensor_VoltageToTemperature(float voltage_v);

/**
 * @brief  Validate measured temprature
 * @param  temperature_c Measured temperature
 * @return true if temperature is valid, false otherwise */
static Sensor_Status_t Sensor_ValidateTemperature(float temperature_c);


/**
 * @brief  Process one ADC block
 * @param  buffer Pointer to ADC sample buffer
 * @param  length Number of ADC samples
 * @return true if processing is successful, false otherwise */
static bool Sensor_ProcessBlock(const uint16_t *buffer, uint16_t length);



/* Public Functions */


/**
 * @brief Initialize the sensor driver
 */
HAL_StatusTypeDef Sensor_Init(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL)
    {
        return HAL_ERROR;
    }
    sensor_hadc = hadc;

    memset(sensor_adc_buffer, 0, sizeof(sensor_adc_buffer));

    sensor_event = SENSOR_EVENT_NONE;
    sensor_data_ready = false;
    sensor_data.adc_raw = 0U;
    sensor_data.voltage_v = 0.0f;
    sensor_data.temperature_c = 0.0f;
    sensor_data.status = SENSOR_STATUS_NO_DATA;

    return HAL_OK;
}


/**
 * @brief Start ADC DMA acquisition 
 */
HAL_StatusTypeDef Sensor_Start(void)
{
    if (sensor_hadc == NULL)
    {
        return HAL_ERROR;
    }

    sensor_event = SENSOR_EVENT_NONE;
    sensor_data_ready = false;

    return HAL_ADC_Start_DMA(sensor_hadc, (uint32_t*)sensor_adc_buffer,
                             SENSOR_ADC_BUFFER_SIZE);

}


/**
 * @brief  Handle ADC DMA half-transfer event
 */
void Sensor_DMA_HalfCompleteCallback(void)
{
    sensor_event = SENSOR_EVENT_HALF_COMPLETE;
}


/**
 * @brief  Handle ADC DMA transfer-complete event
 */
void Sensor_DMA_CompleteCallback(void)
{
    sensor_event = SENSOR_EVENT_COMPLETE;
}


/**
 * @brief Process pending ADC samples
 */
void Sensor_Process(void)
{
    Sensor_Event_t event;

    /*
     * Copy the event atomically with respect to the normal
     * single-core MCU execution model
     */
    event = sensor_event;

    if (event == SENSOR_EVENT_NONE)
    {
        return;
    }

    /*
     * Clear the event before processing
     */
    sensor_event = SENSOR_EVENT_NONE;


    if (event == SENSOR_EVENT_HALF_COMPLETE)
    {
        Sensor_ProcessBlock(&sensor_adc_buffer[0U],SENSOR_ADC_BUFFER_SIZE / 2U);
    }
    else if (event == SENSOR_EVENT_COMPLETE)
    {
        Sensor_ProcessBlock(&sensor_adc_buffer[SENSOR_ADC_BUFFER_SIZE / 2U],
                            SENSOR_ADC_BUFFER_SIZE / 2U);
    }
}


/**
 * @brief Get latest processed sensor data
 */
Sensor_Data_t Sensor_GetData(void)
{
    return sensor_data;
}

/**
 * @brief  Check whether new sensor data is available
 */
bool Sensor_DataReady(void)
{
    bool ready = sensor_data_ready;
    sensor_data_ready = false;
    return ready;
}

/**
 * @brief  Check whether the sensor is currently valid
 */
bool Sensor_IsValid(void)
{
    return (sensor_data.status == SENSOR_STATUS_OK);
}


/**
 * @brief  Get current sensor status
 */
Sensor_Status_t Sensor_GetStatus(void)
{
    return sensor_data.status;
}


/* Private Functions */

/**
 * @brief Calculate arithmetic average of ADC samples
 */
static uint16_t Sensor_CalculateAverage(const uint16_t *buffer,uint16_t length)
{
    uint32_t sum = 0U;

    if ((buffer == NULL) || (length == 0U))
    {
        return 0U;
    }

    for (uint16_t i = 0U; i < length; i++)
    {
        sum += buffer[i];
    }

    return (uint16_t)((sum + (length / 2U)) / length);

}


/**
 * @brief Convert ADC value to voltage 
 */
static float Sensor_ADCToVoltage(uint16_t adc_value)

{
    float sensor_voltage = ((float)adc_value * SENSOR_ADC_VREF) / (float)(SENSOR_ADC_MAX_VALUE);
    return sensor_voltage;
}


/**
 * @brief Convert voltage to temperature
 */
static float Sensor_VoltageToTemperature(float voltage_v)
{
    float temperature_c;
    temperature_c = voltage_v / SENSOR_LM35_SENSITIVITY;
    return temperature_c;
}


/**
 * @brief  Validate temprature
 */
static Sensor_Status_t Sensor_ValidateTemperature(float temperature_c)
{
    if (temperature_c < SENSOR_MIN_TEMPERATURE_C || temperature_c > SENSOR_MAX_TEMPERATURE_C)
    {
        return SENSOR_STATUS_OUT_OF_RANGE;
    }

    return SENSOR_STATUS_OK;
}


/**
 * @brief Process one ADC block   
 */
static bool Sensor_ProcessBlock(const uint16_t *buffer, uint16_t length)
{
    uint16_t adc_average;
    float voltage;
    float temperature;
    Sensor_Status_t status;

    if (buffer == NULL || length == 0U)
    {
        return false;
    }

    /*
     * Step 1:
     * Calculate average ADC value
     */
    adc_average = Sensor_CalculateAverage(buffer, length);

    /*
     * Step 2:
     * Convert ADC value to voltage
     */
    voltage = Sensor_ADCToVoltage(adc_average);

    /*
     * Step 3:
     * Convert voltage to temperature
     */
    temperature = Sensor_VoltageToTemperature(voltage);

    /*
     * Step 4:
     * Validate temperature
     */
    status = Sensor_ValidateTemperature(temperature);

    /*
     * Step 5:
     * Update sensor data structure
     */
    sensor_data.adc_raw = adc_average;
    sensor_data.voltage_v = voltage;
    sensor_data.temperature_c = temperature;
    sensor_data.status = status;

    /*
     * Step 6:
     * Indicate that new data is available
     */
    sensor_data_ready = true;

    return true;
}