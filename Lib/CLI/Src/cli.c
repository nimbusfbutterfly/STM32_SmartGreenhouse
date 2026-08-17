/**
 * @file    cli.c
 * @brief   UART Command Line Interface implementation
 * @details
 * RX architecture:
 *
 *      UART interrupt
 *            |
 *            v
 *      rx_byte
 *            |
 *            v
 *      RX Ring Buffer
 *            |
 *            v
 *      CLI_Process()
 *            |
 *            v
 *      Line Editor
 *            |
 *            v
 *      Tokenizer
 *            |
 *            v
 *      Command Dispatcher
 * @author  Fatemeh Moghadasian
 * @version 2.0
 */

#include "cli.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>


/* Private variables */

/**
 * @brief UART handle used by CLI.
 */
static UART_HandleTypeDef *cli_huart = NULL;

/**
 * @brief Application callback table.
 */
static CLI_Callbacks_t cli_callbacks;


/**
 * @brief Current CLI status.
 */
static volatile CLI_Status_t cli_status = CLI_STATUS_NOT_INITIALIZED;


/**
 * @brief One-byte UART reception variable.
 */
static uint8_t cli_rx_byte;


/**
 * @brief UART RX ring buffer.
 * Producer:
 *      UART interrupt
 * Consumer:
 *      CLI_Process()
 */
static volatile uint8_t cli_rx_ring[CLI_RX_RING_BUFFER_SIZE];

/**
 * @brief Ring buffer write index.
 * Modified only by ISR.
 */
static volatile uint16_t cli_rx_head = 0U;


/**
 * @brief Ring buffer read index.
 * Modified by main context.
 */
static volatile uint16_t cli_rx_tail = 0U;

/**
 * @brief Indicates that RX ring overflow occurred.
 */
static volatile bool cli_rx_overflow = false;

/**
 * @brief Current command line.
 */
static char cli_line_buffer[CLI_LINE_BUFFER_SIZE];

/**
 * @brief Current line length.
 */
static uint16_t cli_line_length = 0U;

/**
 * @brief Previous received character was CR.
 * Used to handle both:
 *      CR
 * and:
 *      CRLF
 */
static bool cli_previous_was_cr = false;


/**
 * @brief Token array.
 */
static char *cli_tokens[CLI_MAX_TOKENS];


/* Private function prototypes */

static void CLI_RearmReception(void);
static void CLI_Send(const char *text);
static void CLI_SendPrompt(void);
static bool CLI_RingPush(uint8_t byte);
static bool CLI_RingPop(uint8_t *byte);
static uint16_t CLI_RingCount(void);
static void CLI_ProcessByte(uint8_t byte);
static void CLI_Backspace(void);
static void CLI_SubmitLine(void);
static uint8_t CLI_Tokenize(char *line, char *tokens[],uint8_t max_tokens);
static void CLI_HandleLine(char *line);
static void CLI_CommandHelp(void);
static void CLI_CommandStatus(void);
static void CLI_CommandTemperature(void);
static void CLI_CommandFan(uint8_t token_count,char *tokens[]);
static void CLI_CommandHeater(uint8_t token_count, char *tokens[]);
static void CLI_CommandSetpoint(uint8_t token_count,char *tokens[]);
static void CLI_CommandMode(uint8_t token_count, char *tokens[]);
static void CLI_CommandClear(void);
static void CLI_CommandReboot(void);
static void CLI_FormatFloat1(float value, char *buffer,uint16_t buffer_size);
static bool CLI_StringEqualIgnoreCase(const char *a, const char *b);
static const char *CLI_ModeToString(CLI_Mode_t mode);
static const char *CLI_StateToString(CLI_SystemState_t state);


/* Public Function */

HAL_StatusTypeDef CLI_Init(UART_HandleTypeDef *huart, const CLI_Callbacks_t *callbacks)
{
    if ((huart == NULL) ||(callbacks == NULL))
    {
        return HAL_ERROR;
    }


    /*
     * These callbacks are mandatory because the current CLI
     * depends on them.
     * SetMode is intentionally optional.
     */
    if ((callbacks->GetSnapshot == NULL) || (callbacks->SetSetpoint == NULL) ||
        (callbacks->SetFanPercent == NULL) || (callbacks->FanOn == NULL) ||
        (callbacks->FanOff == NULL) || (callbacks->HeaterOn == NULL) ||
        (callbacks->HeaterOff == NULL))
    {
        return HAL_ERROR;
    }


    cli_huart = huart;
    cli_callbacks = *callbacks;

    /*
     * Clear RX ring.
     */
    memset((void *)cli_rx_ring, 0, sizeof(cli_rx_ring));

    /*
     * Clear command line.
     */
    memset(cli_line_buffer,0,sizeof(cli_line_buffer));

    /*
     * Reset indices.
     */
    cli_rx_head = 0U;
    cli_rx_tail = 0U;
    cli_line_length = 0U;
    cli_rx_overflow = false;
    cli_previous_was_cr = false;
    cli_status = CLI_STATUS_OK;

    /*
     * Welcome message.
     */
    CLI_Send(
        "\r\n"
        "*****************************************\r\n"
        "       Smart Greenhouse CLI\r\n"
        "*****************************************\r\n"
        "Type 'help' for available commands.\r\n"
    );

    CLI_SendPrompt();

    /*
     * Start interrupt-driven UART reception.
     */
    CLI_RearmReception();

    return HAL_OK;
}


/* CLI Process */

void CLI_Process(void)
{
    uint8_t byte;
    if (cli_status == CLI_STATUS_NOT_INITIALIZED)
    {
        return;
    }

    /*
     * Process all currently available bytes.
     */
    while (CLI_RingPop(&byte))
    {
        CLI_ProcessByte(byte);
    }
}


/* UART RX callback */

void CLI_UART_RxCpltCallback(void)
{
    /*
     * The callback is executed in interrupt context.
     * Keep it extremely short.
     */

    if (cli_status == CLI_STATUS_NOT_INITIALIZED)
    {
        CLI_RearmReception();
        return;
    }


    /*
     * Push received byte into ring buffer.
     */
    if (!CLI_RingPush(cli_rx_byte))
    {
        cli_rx_overflow = true;
        cli_status = CLI_STATUS_RX_OVERFLOW;
    }

    /*
     * Immediately arm reception of next byte.
     */
    CLI_RearmReception();
}


/* UART Error Callback */

void CLI_UART_ErrorCallback(void)
{
    /*
     * UART error occurred.
     *
     * Do not perform recovery-heavy processing here.
     */
    cli_status = CLI_STATUS_UART_ERROR;


    /*
     * Try to continue reception.
     */
    CLI_RearmReception();
}


/* Status */

CLI_Status_t CLI_GetStatus(void)
{
    return cli_status;
}


/* RX Pending */

uint16_t CLI_GetRxPending(void)
{
    return CLI_RingCount();
}


/* RX Overflow */

bool CLI_HasRxOverflow(void)
{
    return cli_rx_overflow;
}

/* Public: Clear Overflow */

void CLI_ClearRxOverflow(void)
{
    /*
     * Clear error condition.
     */
    cli_rx_overflow = false;


    if (cli_status == CLI_STATUS_RX_OVERFLOW)
    {
        cli_status = CLI_STATUS_OK;
    }
}


/* Print */

void CLI_Print(const char *text)
{
    CLI_Send(text);
}


/*Print Status */

void CLI_PrintStatus(void)
{
    CLI_CommandStatus();
}


/* UART */

static void CLI_RearmReception(void)
{
    if (cli_huart == NULL)
    {
        return;
    }

    /*
     * Receive exactly one byte.
     */
    (void)HAL_UART_Receive_IT(cli_huart, &cli_rx_byte, 1U);
}


/* UART TX */

static void CLI_Send(const char *text)
{
    if ((cli_huart == NULL) ||(text == NULL))
    {
        return;
    }

    /*
     * Blocking TX.
     * This function is called only from normal context.
     */
    (void)HAL_UART_Transmit(cli_huart,(uint8_t *)text,
                            (uint16_t)strlen(text), CLI_TX_TIMEOUT_MS);
}


/* Prompt */

static void CLI_SendPrompt(void)
{
    CLI_Send( "greenhouse> ");
}


/* Ring Buffer - Push */

static bool CLI_RingPush(uint8_t byte)
{
    uint16_t next_head;

    /*
     * Because buffer size is 256, the following expression
     * naturally wraps from 255 to 0.
     */
    next_head =(uint16_t)((cli_rx_head + 1U)% CLI_RX_RING_BUFFER_SIZE);

    /*
     * Buffer full.
     */
    if (next_head == cli_rx_tail)
    {
        return false;
    }

    /*
     * Store byte.
     */
    cli_rx_ring[cli_rx_head] = byte;


    /*
     * Publish new head.
     */
    cli_rx_head = next_head;


    return true;
}


/* Ring Buffer - Pop */

static bool CLI_RingPop(uint8_t *byte)
{
    uint16_t tail;

    if (byte == NULL)
    {
        return false;
    }

    /*
     * Empty buffer.
     */
    if (cli_rx_tail == cli_rx_head)
    {
        return false;
    }

    tail = cli_rx_tail;

    /*
     * Read byte.
     */
    *byte = cli_rx_ring[tail];

    /*
     * Advance tail.
     */
    cli_rx_tail =(uint16_t)((tail + 1U) % CLI_RX_RING_BUFFER_SIZE);

    return true;
}


/* Ring Buffer - Count */

static uint16_t CLI_RingCount(void)
{
    uint16_t head;
    uint16_t tail;

    head = cli_rx_head;
    tail = cli_rx_tail;

    if (head >= tail)
    {
        return head - tail;
    }

    return(uint16_t)(CLI_RX_RING_BUFFER_SIZE - tail + head );
}


/* Byte Processing */

static void CLI_ProcessByte(uint8_t byte)
{
    char received =(char)byte;

    /* ENTER */

    if ((received == '\r') ||(received == '\n'))
    {
        /*
         * Handle CRLF.
         * If CR already submitted the command, the following
         * LF must not submit an empty second command.
         */
        if ((received == '\n') && cli_previous_was_cr)
        {
            cli_previous_was_cr = false;
            return;
        }


        cli_previous_was_cr = (received == '\r');
        CLI_SubmitLine();

        return;
    }

    cli_previous_was_cr = false;


    /* BACKSPACE */

    if ((received == '\b') || (received == 0x7FU))
    {
        CLI_Backspace();
        return;
    }


    /* Printable character */

    if (isprint((unsigned char)received))
    {
        if (cli_line_length <(CLI_LINE_BUFFER_SIZE - 1U))
        {
            cli_line_buffer[cli_line_length] = received;
            cli_line_length++;

            /*
             * Echo character.
             */
            char echo[2];

            echo[0] = received;
            echo[1] = '\0';

            CLI_Send(echo);
        }
        else
        {
            /*
             * Line too long.
             */
            CLI_Send("\r\nERROR: command line too long.\r\n");
            cli_line_length = 0U;
            cli_line_buffer[0] = '\0';
            CLI_SendPrompt();
        }
    }
}


/* Backspace */

static void CLI_Backspace(void)
{
    if (cli_line_length == 0U)
    {
        return;
    }

    cli_line_length--;

    cli_line_buffer[cli_line_length] = '\0';


    /*
     * Terminal sequence:
     *   backspace
     *   space
     *   backspace
     * visually removes the last character.
     */
    CLI_Send("\b \b");
}


/* Submit Line */

static void CLI_SubmitLine(void)
{
    /*
     * Echo newline.
     */
    CLI_Send("\r\n");

    /*
     * Empty command.
     */
    if (cli_line_length == 0U)
    {
        CLI_SendPrompt();
        return;
    }


    /*
     * Null terminate.
     */
    cli_line_buffer[cli_line_length] = '\0';

    /*
     * Execute command.
     */
    CLI_HandleLine(cli_line_buffer
    );

    /*
     * Clear line buffer.
     */
    memset(cli_line_buffer,0,sizeof(cli_line_buffer));

    cli_line_length = 0U;

    /*
     * New prompt.
     */
    CLI_SendPrompt();
}


/* Tokenizer */

static uint8_t CLI_Tokenize(char *line, char *tokens[],uint8_t max_tokens)
{
    uint8_t count = 0U;
    char *ptr = line;

    if ((line == NULL) || (tokens == NULL) || (max_tokens == 0U))
    {
        return 0U;
    }


    while ((*ptr != '\0') &&(count < max_tokens))
    {
        /*
         * Skip spaces and tabs.
         */
        while ((*ptr == ' ') || (*ptr == '\t'))
        {
            ptr++;
        }

        if (*ptr == '\0')
        {
            break;
        }

        /*
         * Start token.
         */
        tokens[count] = ptr;
        count++;


        /*
         * Search token end.
         */
        while ((*ptr != '\0') && (*ptr != ' ') &&(*ptr != '\t'))
        {
            ptr++;
        }


        /*
         * Replace separator with NULL.
         */
        if (*ptr != '\0')
        {
            *ptr = '\0';
            ptr++;
        }
    }


    return count;
}


/* Case-Insensitive String Comparison */

static bool CLI_StringEqualIgnoreCase(const char *a, const char *b)
{
    if ((a == NULL) ||(b == NULL))
    {
        return false;
    }


    while ((*a != '\0') &&(*b != '\0'))
    {
        if (tolower((unsigned char)*a) != tolower( (unsigned char)*b))
        {
            return false;
        }

        a++;
        b++;
    }

    return(*a == '\0') && (*b == '\0');
}


/* Command Dispatcher */

static void CLI_HandleLine(char *line)
{
    uint8_t count;
    count = CLI_Tokenize(line, cli_tokens,CLI_MAX_TOKENS);

    if (count == 0U)
    {
        return;
    }

    /* HELP */
    if (CLI_StringEqualIgnoreCase(cli_tokens[0], "help"))
    {
        CLI_CommandHelp();
    }


    /* STATUS */
    else if (CLI_StringEqualIgnoreCase( cli_tokens[0], "status"))
    {
        CLI_CommandStatus();
    }


    /* TEMPERATURE */
    else if (CLI_StringEqualIgnoreCase(cli_tokens[0], "temperature") ||
             CLI_StringEqualIgnoreCase(cli_tokens[0], "temp"))
    {
        CLI_CommandTemperature();
    }


    /* FAN */
    else if (CLI_StringEqualIgnoreCase(cli_tokens[0], "fan"))
    {
        CLI_CommandFan(count, cli_tokens);
    }


    /* HEATER */
    else if (CLI_StringEqualIgnoreCase(cli_tokens[0], "heater"))
    {
        CLI_CommandHeater(count, cli_tokens);
    }


    /* SETPOINT */
    else if (CLI_StringEqualIgnoreCase(cli_tokens[0], "setpoint"))
    {
        CLI_CommandSetpoint(count,cli_tokens);
    }


    /* MODE */
    else if (CLI_StringEqualIgnoreCase(cli_tokens[0], "mode"))
    {
        CLI_CommandMode(count,cli_tokens);
    }


    /* CLEAR */
    else if (CLI_StringEqualIgnoreCase(cli_tokens[0],"clear"))
    {
        CLI_CommandClear();
    }


    /* REBOOT */

    else if (CLI_StringEqualIgnoreCase(cli_tokens[0], "reboot"))
    {
        CLI_CommandReboot();
    }

    /* UNKNOWN */
    else
    {
        CLI_Send("ERROR: unknown command: ");
        CLI_Send(cli_tokens[0]);
        CLI_Send("\r\n""Type 'help' for available commands.\r\n");
    }
}


/* HELP */

static void CLI_CommandHelp(void)
{
    CLI_Send(
        "\r\n"
        "Smart Greenhouse CLI\r\n"
        "*****************************************************\r\n"
        "help\r\n"
        "  Show available commands.\r\n"
        "\r\n"
        "status\r\n"
        "  Show complete system status.\r\n"
        "\r\n"
        "temperature\r\n"
        "  Show current temperature.\r\n"
        "\r\n"
        "fan <0..100>\r\n"
        "  Set fan PWM percentage.\r\n"
        "\r\n"
        "fan on\r\n"
        "  Turn fan ON using current speed.\r\n"
        "\r\n"
        "fan off\r\n"
        "  Turn fan OFF.\r\n"
        "\r\n"
        "heater on\r\n"
        "  Turn heater ON.\r\n"
        "\r\n"
        "heater off\r\n"
        "  Turn heater OFF.\r\n"
        "\r\n"
        "setpoint <0..50>\r\n"
        "  Change temperature setpoint.\r\n"
        "\r\n"
        "mode\r\n"
        "  Show current operating mode.\r\n"
        "\r\n"
        "mode auto\r\n"
        "  Select automatic mode.\r\n"
        "\r\n"
        "mode manual\r\n"
        "  Select manual mode.\r\n"
        "\r\n"
        "clear\r\n"
        "  Clear terminal screen.\r\n"
        "\r\n"
        "reboot\r\n"
        "  Reset MCU through software.\r\n"
        "*****************************************************\r\n"
    );
}


/* STATUS */

static void CLI_CommandStatus(void)
{
    CLI_SystemSnapshot_t snapshot;
    char line[128];
    char temperature[16];
    char setpoint[16];

    memset( &snapshot, 0,sizeof(snapshot));

    cli_callbacks.GetSnapshot(&snapshot);


    /* Header*/

    CLI_Send("\r\n" "^************ SYSTEM STATUS ************^\r\n");

    /* Sensor */

    if (snapshot.sensor_valid)
    {
        CLI_FormatFloat1(snapshot.temperature_c,temperature, sizeof(temperature));

        snprintf(line, sizeof(line), "Temperature : %s C\r\n", temperature);
    }
    else
    {
        snprintf(line, sizeof(line), "Temperature : INVALID\r\n");
    }

    CLI_Send(line);

    /* ADC */

    snprintf(line, sizeof(line), "ADC         : %u\r\n", (unsigned int)snapshot.adc_raw);
    CLI_Send(line);

    /* Sensor Voltage*/

    CLI_FormatFloat1( snapshot.sensor_voltage_v, temperature,sizeof(temperature));

    snprintf(line, sizeof(line), "Sensor V    : %s V\r\n", temperature);
    CLI_Send(line);

    /* Setpoint */

    CLI_FormatFloat1( snapshot.setpoint_c, setpoint, sizeof(setpoint));

    snprintf(line,sizeof(line), "Setpoint    : %s C\r\n",setpoint);
    CLI_Send(line);

    /* Heater */

    snprintf(line, sizeof(line), "Heater      : %s\r\n", snapshot.heater_on ? "ON": "OFF");

    CLI_Send(line);


    /* Fan */

    snprintf(line, sizeof(line), "Fan         : %u %%\r\n", (unsigned int)snapshot.fan_speed_percent);

    CLI_Send(line);

    /* Mode */

    snprintf(line, sizeof(line), "Mode        : %s\r\n", snapshot.mode_name != NULL ? snapshot.mode_name
                        : CLI_ModeToString(snapshot.mode));

    CLI_Send(line);

    /* State */

    snprintf(line, sizeof(line), "State       : %s\r\n", snapshot.system_state_name != NULL
             ? snapshot.system_state_name : CLI_StateToString(snapshot.system_state));

    CLI_Send(line);

    CLI_Send("*********************************\r\n");
}


/* TEMPERATURE */

static void CLI_CommandTemperature(void)
{
    CLI_SystemSnapshot_t snapshot;
    char line[96];
    char temperature[16];

    memset(&snapshot, 0, sizeof(snapshot));

    cli_callbacks.GetSnapshot(&snapshot);

    if (!snapshot.sensor_valid)
    {
        CLI_Send("Temperature sensor: INVALID\r\n");
        return;
    }

    CLI_FormatFloat1(snapshot.temperature_c,temperature,sizeof(temperature));

    snprintf(line, sizeof(line), "Temperature : %s C\r\n", temperature);
    CLI_Send(line);
}


/* FAN COMMAND */

static void CLI_CommandFan(uint8_t token_count,char *tokens[])
{
    char *end_ptr;
    long value;

    if (token_count < 2U)
    {
        CLI_Send("Usage:\r\n" "  fan <0..100>\r\n" "  fan on\r\n" "  fan off\r\n");
        return;
    }


    /* FAN ON */
    if (CLI_StringEqualIgnoreCase(tokens[1],"on"))
    {
        if (cli_callbacks.FanOn())
        {
            CLI_Send("Fan: ON\r\n");
        }
        else
        {
            CLI_Send("ERROR: fan ON rejected.\r\n");
        }
        return;
    }


    /* FAN OFF */
    if (CLI_StringEqualIgnoreCase(tokens[1],"off"))
    {
        if (cli_callbacks.FanOff())
        {
            CLI_Send("Fan: OFF\r\n");
        }
        else
        {
            CLI_Send("ERROR: fan OFF rejected.\r\n");
        }

        return;
    }


    /* FAN PERCENTAGE */

    end_ptr = NULL;
    value = strtol(tokens[1],&end_ptr,10);

    if ((end_ptr == tokens[1]) ||(*end_ptr != '\0'))
    {
        CLI_Send("ERROR: invalid fan percentage.\r\n");
        return;
    }

    if ((value < 0L) || (value > 100L))
    {
        CLI_Send( "ERROR: fan percentage must be 0..100.\r\n");
        return;
    }


    if (cli_callbacks.SetFanPercent((uint8_t)value))
    {
        char line[64];

        snprintf(line,sizeof(line),"Fan PWM set to %ld%%\r\n",value);
        CLI_Send(line);
    }
    else
    {
        CLI_Send("ERROR: fan command rejected.\r\n");
    }
}


/* HEATER COMMAND */

static void CLI_CommandHeater(uint8_t token_count,char *tokens[])
{
    if (token_count < 2U)
    {
        CLI_Send("Usage:\r\n" "  heater on\r\n""  heater off\r\n");
        return;
    }


    /* HEATER ON */

    if (CLI_StringEqualIgnoreCase(tokens[1],"on"))
    {
        if (cli_callbacks.HeaterOn())
        {
            CLI_Send("Heater: ON\r\n");
        }
        else
        {
            CLI_Send("ERROR: heater ON rejected.\r\n");
        }

        return;
    }

    /* HEATER OFF */

    if (CLI_StringEqualIgnoreCase(tokens[1],"off"))
    {
        if (cli_callbacks.HeaterOff())
        {
            CLI_Send("Heater: OFF\r\n");
        }
        else
        {
            CLI_Send("ERROR: heater OFF rejected.\r\n"
            );
        }
        return;
    }

    CLI_Send("ERROR: use 'heater on' or 'heater off'.\r\n");
}


/* SETPOINT */

static void CLI_CommandSetpoint(uint8_t token_count, char *tokens[]
)
{
    char *end_ptr;
    float value;
    char formatted[16];
    char line[64];

    if (token_count < 2U)
    {
        CLI_Send("Usage: setpoint <temperature>\r\n");
        return;
    }


    end_ptr = NULL;


    value = strtof(tokens[1], &end_ptr);


    if ((end_ptr == tokens[1]) ||(*end_ptr != '\0'))
    {
        CLI_Send("ERROR: invalid temperature.\r\n");
        return;
    }


    if ((value < CLI_SETPOINT_MIN_C) ||(value > CLI_SETPOINT_MAX_C))
    {
        CLI_Send("ERROR: setpoint range is 0..50 C.\r\n");
        return;
    }


    if (cli_callbacks.SetSetpoint(value))
    {
        CLI_FormatFloat1(value, formatted,sizeof(formatted));


        snprintf(line,sizeof(line),"Setpoint updated to %s C\r\n", formatted);
        CLI_Send(line);
    }
    else
    {
        CLI_Send("ERROR: setpoint command rejected.\r\n");
    }
}


/* MODE*/

static void CLI_CommandMode(uint8_t token_count, char *tokens[])
{
    CLI_SystemSnapshot_t snapshot;

    /*
     * No argument:
     * mode
     * means display current mode.
     */
    if (token_count < 2U)
    {
        memset(&snapshot, 0, sizeof(snapshot));

        cli_callbacks.GetSnapshot(&snapshot);

        CLI_Send("Mode: ");

        if (snapshot.mode_name != NULL)
        {
            CLI_Send(snapshot.mode_name);
        }
        else
        {
            CLI_Send(CLI_ModeToString(snapshot.mode));
        }

        CLI_Send("\r\n");
        return;
    }


    /*
     * Mode control is optional.
     */
    if (cli_callbacks.SetMode == NULL)
    {
        CLI_Send("ERROR: mode control is not available.\r\n");
        return;
    }

    /* AUTO */
    if (CLI_StringEqualIgnoreCase(tokens[1], "auto"))
    {
        if (cli_callbacks.SetMode(CLI_MODE_AUTO))
        {
            CLI_Send("Mode set to AUTO.\r\n");
        }
        else
        {
            CLI_Send("ERROR: mode change rejected.\r\n");
        }

        return;
    }


    /* MANUAL */
    if (CLI_StringEqualIgnoreCase(tokens[1],"manual"))
    {
        if (cli_callbacks.SetMode(CLI_MODE_MANUAL))
        {
            CLI_Send("Mode set to MANUAL.\r\n");
        }
        else
        {
            CLI_Send("ERROR: mode change rejected.\r\n");
        }

        return;
    }


    CLI_Send("ERROR: valid modes are 'auto' and 'manual'.\r\n");
}


/* CLEAR */

static void CLI_CommandClear(void)
{
    CLI_Send(
        "\033[2J\033[H"
    );
}


/* REBOOT */

static void CLI_CommandReboot(void)
{
    CLI_Send("System rebooting...\r\n");

    /*
     * Give UART time to transmit.
     */
    HAL_Delay(50U);


    /*
     * Software reset.
     */
    NVIC_SystemReset();
}


/* Float Formatter */

static void CLI_FormatFloat1(float value, char *buffer,uint16_t buffer_size)
{
    bool negative = false;
    long scaled;
    long whole;
    long fraction;

    if ((buffer == NULL) ||(buffer_size == 0U))
    {
        return;
    }

    if (value < 0.0f)
    {
        negative = true;
        value = -value;
    }


    /*
     * One decimal digit.
     * Example:
     * 24.37 -> 244
     */
    scaled =(long)((value * 10.0f)+ 0.5f );

    whole = scaled / 10L;
    fraction = scaled % 10L;

    snprintf(buffer, buffer_size, "%s%ld.%ld", negative ? "-" : "", whole, fraction);
}


/* Mode String */

static const char *CLI_ModeToString(CLI_Mode_t mode)
{
    switch (mode)
    {
        case CLI_MODE_AUTO:
            return "AUTO";

        case CLI_MODE_MANUAL:
            return "MANUAL";

        case CLI_MODE_WARNING:
            return "WARNING";

        case CLI_MODE_FAULT:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}


/* State String */

static const char *CLI_StateToString(CLI_SystemState_t state)
{
    switch (state)
    {
        case CLI_SYSTEM_STATE_INIT:
            return "INIT";

        case CLI_SYSTEM_STATE_NORMAL:
            return "NORMAL";

        case CLI_SYSTEM_STATE_WARNING:
            return "WARNING";

        case CLI_SYSTEM_STATE_SENSOR_FAULT:
            return "SENSOR_FAULT";

        case CLI_SYSTEM_STATE_ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}