# STM32 Smart Greenhouse 🌱

A modular embedded smart greenhouse monitoring and control system based on the **STM32F446** microcontroller.

The system continuously monitors greenhouse temperature using an **LM35 analog temperature sensor** and controls environmental conditions through a **PWM-controlled fan** and a **heater relay**. A character LCD provides local status information, while a UART-based Command Line Interface (CLI) provides a user interface for configuration, monitoring, and system control.

---

## Overview

The STM32 Smart Greenhouse is an embedded control system designed to monitor and regulate the temperature of a small greenhouse environment.

The main objective of the project is to implement a modular and maintainable embedded software architecture in which hardware drivers are separated from application-level control logic.

The system consists of:

- Temperature sensing using an LM35 sensor
- Automatic heater control
- Automatic cooling fan control
- PWM-based fan speed control
- Character LCD status display
- Buzzer alarm
- UART Command Line Interface (CLI)
- Modular C-based software architecture
- CMake-based build system
- STM32 HAL-based hardware abstraction

The firmware is designed around a layered architecture:

```text
+--------------------------------------------------+
|                 User Interface                   |
|                                                  |
|       LCD                  UART CLI              |
+-------------------------+------------------------+
                          |
                          v
+--------------------------------------------------+
|                Application Layer                 |
|                                                  |
|                    App                           |
|                                                  |
|        Temperature / Control / Alarm Logic       |
+--------------------------------------------------+
                          |
                          v
+--------------------------------------------------+
|                 Driver Layer                     |
|                                                  |
| Sensor | Fan | Heater | LCD | CLI                |
+--------------------------------------------------+
                          |
                          v
+--------------------------------------------------+
|              STM32 HAL / CMSIS                   |
+--------------------------------------------------+
                          |
                          v
+--------------------------------------------------+
|                  STM32F446                       |
+--------------------------------------------------+
```

---

## Features

### Temperature Monitoring

The system measures greenhouse temperature using an **LM35 analog temperature sensor** connected to an ADC input of the STM32.

The measured analog voltage is converted into temperature using the LM35 transfer characteristic.

The firmware internally supports temperature values with a resolution of **0.1 °C**.

Example:

```text
25.0 °C
25.1 °C
25.2 °C
```

### Automatic Temperature Control

The greenhouse temperature is controlled using a setpoint.

Depending on the measured temperature, the system can activate:

- Heater
- Fan
- Buzzer alarm

The control behavior is implemented at the application layer rather than inside the low-level hardware drivers.

This separation allows the hardware drivers to remain reusable.

### PWM Fan Control

The cooling fan is controlled using PWM.

PWM allows the firmware to control the effective fan power/speed without directly manipulating the motor hardware from the application layer.

### Heater Control

The heater is controlled through a digital output and relay interface.

The application determines whether the heater should be active according to the measured temperature and configured setpoint.

### LCD Display

A character LCD is used as the local user interface.

The LCD can display information such as:

```text
Temperature
Setpoint
System mode
Warning status
```

### Buzzer Alarm

A buzzer is used to provide an audible warning when an abnormal or alarm condition is detected.

The buzzer logic is controlled by the application layer while the GPIO-level implementation remains isolated from the control algorithm.

### UART Command Line Interface

The system includes a UART-based Command Line Interface (CLI).

The CLI allows the user to interact with the greenhouse controller through a serial terminal.

The architecture uses an RX buffer so that UART reception does not block the main application loop.

Conceptually:

```text
 UART RX Interrupt
        |
        v
  RX Ring Buffer
        |
        v
  CLI_Process()
        |
        v
  Command Parser
        |
        v
    Application
```

---

## System Architecture

The software architecture is organized into three major layers.

### 1. Application Layer

Located in:

```text
App/
```

The application layer coordinates all system components.

It is responsible for:

- Reading temperature
- Applying temperature control logic
- Managing the setpoint
- Controlling operating mode
- Managing alarms
- Updating the LCD
- Coordinating fan and heater operation

The application layer should not directly manipulate STM32 GPIO or ADC registers.

### 2. Driver Layer

Located in:

```text
Lib/
```

The driver layer contains reusable modules for individual hardware components.

Current drivers include:

```text
Sensor
Fan
Heater
LCD
CLI
```

Each driver exposes a clean interface through its header file.

### 3. STM32 Hardware Layer

The low-level STM32 support is provided by:

```text
Drivers/CMSIS/
Drivers/STM32F4xx_HAL_Driver/
Core/
```

The project uses the STM32 HAL to abstract peripheral access.

---

## Hardware

### Main Microcontroller

```text
MCU: STM32F446
Architecture: ARM Cortex-M4
```

The STM32F446 provides:

- ADC
- GPIO
- Timers
- PWM
- UART
- System timing

### Temperature Sensor

#### LM35

The LM35 is an analog temperature sensor.

Its output voltage is approximately:

```text
10 mV / °C
```

Therefore:

```text
Vout = Temperature × 10 mV
```

For example:

```text
Temperature = 25 °C

Vout ≈ 250 mV
```

The STM32 ADC measures the sensor voltage and the firmware converts the ADC result into temperature.

### Fan

The fan is controlled using a PWM signal generated by an STM32 timer.

Conceptually:

```text
STM32 Timer
     |
     | PWM
     v
Fan Driver / Transistor
     |
     v
   Fan
```

### Heater

The heater is controlled using a digital GPIO output connected to a relay or switching stage.

```text
STM32 GPIO
     |
     v
Relay / Driver
     |
     v
  Heater
```

### LCD

The character LCD provides local system information.

The LCD driver abstracts the low-level communication protocol from the application.

### Buzzer

The buzzer is controlled through a GPIO output.

It is activated by the application when an alarm condition occurs.

---

## Software Architecture

The project follows a modular embedded software architecture.

```text
                     +----------------+
                     |     main.c     |
                     +--------+-------+
                              |
                              v
                     +----------------+
                     |      App       |
                     +--------+-------+
                              |
          +-------------------+-------------------+
          |          |          |        |        |
          v          v          v        v        v
       Sensor       Fan      Heater     LCD      CLI
          |          |          |        |        |
          +----------+----------+--------+--------+
                              |
                              v
                     STM32 HAL / CMSIS
                              |
                              v
                         STM32F446
```

The primary design principle is separation of concerns.

For example:

```c
Sensor_ReadTemperature();
```

should be responsible for temperature acquisition.

It should not decide:

```c
if (temperature > setpoint)
    Fan_On();
```

That decision belongs to the application layer.

---

## Project Structure

```text
STM32-Smart-Greenhouse/
│
├── App/
│   ├── Inc/
│   │   └── app.h
│   └── Src/
│       └── app.c
│
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── stm32f4xx_hal_conf.h
│   │   └── stm32f4xx_it.h
│   └── Src/
│       ├── main.c
│       ├── stm32f4xx_hal_msp.c
│       ├── stm32f4xx_it.c
│       ├── syscalls.c
│       ├── sysmem.c
│       └── system_stm32f4xx.c
│
├── Drivers/
│   ├── BSP/
│   │   └── STM32F4xx-Nucleo/
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
│
├── Lib/
│   ├── CLI/
│   │   ├── Inc/
│   │   └── Src/
│   ├── Fan/
│   │   ├── Inc/
│   │   └── Src/
│   ├── Heater/
│   │   ├── Inc/
│   │   └── Src/
│   ├── LCD/
│   │   ├── Inc/
│   │   └── Src/
│   └── Sensor/
│       ├── Inc/
│       └── Src/
│
├── cmake/
│   ├── gcc-arm-none-eabi.cmake
│   ├── starm-clang.cmake
│   └── stm32cubemx/
│       └── CMakeLists.txt
│
├── CMakeLists.txt
├── CMakePresets.json
├── smartGreenhouse.ioc
├── startup_stm32f446xx.s
├── STM32F446xx_FLASH.ld
├── README.md
└── .gitignore
```

---

## Temperature Measurement

The temperature measurement chain is:

```text
LM35
 |
 | Analog voltage
 v
STM32 ADC
 |
 v
ADC Raw Value
 |
 v
Voltage Conversion
 |
 v
Temperature Calculation
 |
 v
Application
```

The ADC measurement is converted to voltage using the configured ADC reference voltage.

The LM35 transfer function is then used to calculate temperature.

A typical relationship is:

```text
Temperature (°C) = Voltage (mV) / 10
```

The implementation keeps the sensor driver independent from the application control algorithm.

---

## Control Logic

The application continuously evaluates the measured temperature.

A simplified control strategy is:

```text
              +-------------------+
              | Read Temperature   |
              +---------+---------+
                        |
                        v
              +-------------------+
              | Compare with      |
              | Temperature       |
              | Setpoint          |
              +---------+---------+
                        |
              +---------+---------+
              |                   |
              v                   v
        Temperature low     Temperature high
              |                   |
              v                   v
           Heater ON            Fan ON
              |                   |
              +---------+---------+
                        |
                        v
                  Update LCD
                        |
                        v
                  Check Alarm
```

The exact thresholds and hysteresis behavior are defined by the application implementation.

---

## Hysteresis

A temperature controller should ideally use hysteresis to avoid rapid switching between heating and cooling states.

For example, if:

```text
Setpoint = 25 °C
Hysteresis = 1 °C
```

the controller can operate approximately as:

```text
Temperature < 24 °C
        |
        v
    Heater ON

24 °C <= Temperature <= 26 °C
        |
        v
    Normal region

Temperature > 26 °C
        |
        v
      Fan ON
```

Hysteresis prevents excessive relay switching when the temperature fluctuates around the setpoint.

---

## Fan Control

The fan driver provides an abstraction for PWM control.

Conceptually:

```c
Fan_Init();
Fan_SetSpeed(percent);
Fan_On();
Fan_Off();
```

The application should communicate with the fan through these interfaces instead of directly accessing the STM32 timer registers.

PWM duty cycle determines the commanded fan power.

For example:

```text
0%   -> Fan OFF
25%  -> Low speed
50%  -> Medium speed
75%  -> High speed
100% -> Maximum commanded speed
```

The actual relationship between duty cycle and physical fan RPM depends on the fan and its driver circuit.

---

## Heater Control

The heater driver provides a simple interface to the application.

Conceptually:

```c
Heater_Init();
Heater_On();
Heater_Off();
Heater_GetState();
```

The application determines when heating is required.

The driver is responsible only for translating the requested state into the appropriate GPIO output.

---

## LCD Interface

The LCD is used for local monitoring.

A typical display can contain:

```text
T: 25.4 C
SP: 25.0 C
MODE: AUTO
```

During an alarm condition, the display can provide warning information.

The LCD driver isolates the application from the LCD communication details.

---

## Buzzer Alarm

The buzzer provides an audible indication of an alarm condition.

The application determines whether the buzzer should be active.

The driver handles the hardware-level control.

Conceptually:

```c
Buzzer_On();
Buzzer_Off();
```

An important design goal is that alarm logic remains in the application layer rather than being embedded in the low-level GPIO implementation.

---

## UART CLI

The UART CLI provides an interactive interface for monitoring and configuring the greenhouse controller.

The CLI architecture is designed to avoid blocking the main application loop.

```text
                 UART
                   |
                   v
            RX Interrupt
                   |
                   v
             Ring Buffer
                   |
                   v
            CLI_Process()
                   |
                   v
           Command Parser
                   |
                   v
              Application
```

This architecture allows UART reception to occur asynchronously while the main loop continues executing.

---

## CLI Commands

The CLI provides commands for interacting with the system.

Typical commands include:

```text
help
```

Displays the available commands.

Example:

```text
greenhouse> help
```

---

```text
status
```

Displays the current greenhouse state.

Possible information includes:

```text
Temperature
Setpoint
Mode
Fan state
Heater state
Alarm state
```

---

```text
setpoint <value>
```

Changes the desired temperature setpoint.

Example:

```text
greenhouse> setpoint 25.0
```

---

```text
mode auto
```

Enables automatic control.

Example:

```text
greenhouse> mode auto
```

---

```text
mode manual
```

Enables manual control.

Example:

```text
greenhouse> mode manual
```

---

The exact command set is defined by the current CLI implementation.

Use:

```text
help
```

inside the terminal to obtain the commands supported by the firmware version.

---

## Build System

The project uses **CMake** for build configuration and the **GNU Arm Embedded Toolchain** for cross-compilation.

The build system is organized around:

```text
CMakeLists.txt
CMakePresets.json
cmake/
```

The firmware targets the ARM Cortex-M4 architecture used by the STM32F446.

---

### Build

```bash
cmake --build --preset Release
```

Alternatively, if the build directory has already been configured:

```bash
cmake --build build/Release
```

The generated firmware artifacts are placed in the build directory.

Build directories are intentionally excluded from Git using `.gitignore`.

---

## System Operation

The application follows a cooperative embedded execution model.

Conceptually:

```c
int main(void)
{
    ...
    App_Init();

    while (1)
    {
        App_Process();
    }
    ...
}
```

